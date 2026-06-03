/*
 * This file is part of the Carvera Firmware Simulator.
 *
 * Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SIMULATOR_TESTS_SUPPORT_STREAM_STDIO_HARNESS_HPP
#define SIMULATOR_TESTS_SUPPORT_STREAM_STDIO_HARNESS_HPP

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "carvera_sim.pb.h"
#include "framed_proto_client.hpp"

#include <fcntl.h>

namespace sim::test {

constexpr auto kStreamRequestTimeout = std::chrono::seconds(30);

class StreamStdioHarness {
 public:
  explicit StreamStdioHarness(const char* executable) : executable_(executable) {}

  ~StreamStdioHarness() { stop(); }

  StreamStdioHarness(const StreamStdioHarness&) = delete;
  StreamStdioHarness& operator=(const StreamStdioHarness&) = delete;

  bool start() {
    int to_child[2]{};
    int from_child[2]{};
    int err_child[2]{};
    if (pipe(to_child) != 0 || pipe(from_child) != 0 || pipe(err_child) != 0) {
      std::cerr << "pipe failed: " << std::strerror(errno) << '\n';
      return false;
    }

    child_ = fork();
    if (child_ < 0) {
      std::cerr << "fork failed: " << std::strerror(errno) << '\n';
      return false;
    }

    if (child_ == 0) {
      dup2(to_child[0], STDIN_FILENO);
      dup2(from_child[1], STDOUT_FILENO);
      dup2(err_child[1], STDERR_FILENO);
      close(to_child[0]);
      close(to_child[1]);
      close(from_child[0]);
      close(from_child[1]);
      close(err_child[0]);
      close(err_child[1]);
      execl(executable_.c_str(), executable_.c_str(), nullptr);
      _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);
    close(err_child[1]);
    to_child_ = to_child[1];
    from_child_ = from_child[0];
    err_child_ = err_child[0];
    const int flags = fcntl(err_child_, F_GETFL, 0);
    if (flags >= 0) {
      fcntl(err_child_, F_SETFL, flags | O_NONBLOCK);
    }
    reader_ = std::thread([this] { read_loop(); });
    return true;
  }

  void stop(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    if (to_child_ >= 0) {
      close(to_child_);
      to_child_ = -1;
    }

    if (child_ > 0) {
      const auto deadline = std::chrono::steady_clock::now() + timeout;
      int status = 0;
      while (std::chrono::steady_clock::now() < deadline) {
        const auto waited = waitpid(child_, &status, WNOHANG);
        if (waited == child_) {
          child_ = -1;
          break;
        }
        if (waited < 0) {
          child_ = -1;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (child_ > 0) {
        kill(child_, SIGTERM);
        waitpid(child_, &status, 0);
        child_ = -1;
      }
    }

    if (reader_.joinable()) {
      reader_.join();
    }
    if (from_child_ >= 0) {
      close(from_child_);
      from_child_ = -1;
    }
    if (err_child_ >= 0) {
      stderr_output_ += read_all(err_child_);
      close(err_child_);
      err_child_ = -1;
    }
  }

  bool write_request(const carvera::sim::v1::Request& request) {
    std::lock_guard lock(write_mutex_);
    return to_child_ >= 0 && write_framed_message(to_child_, request);
  }

  bool wait_response(std::uint64_t request_id, carvera::sim::v1::Response& response,
                     std::chrono::milliseconds timeout = kStreamRequestTimeout) {
    return wait_until(
        [&](const std::vector<carvera::sim::v1::StreamFrame>& frames) {
          for (const auto& frame : frames) {
            if (frame.payload_case() == carvera::sim::v1::StreamFrame::kResponse &&
                frame.response().id() == request_id) {
              response = frame.response();
              return true;
            }
          }
          return false;
        },
        timeout);
  }

  bool request(carvera::sim::v1::Request& request, std::uint64_t request_id, carvera::sim::v1::Response& response,
               std::chrono::milliseconds timeout = kStreamRequestTimeout) {
    request.set_id(request_id);
    return write_request(request) && wait_response(request_id, response, timeout);
  }

  bool request_ok(carvera::sim::v1::Request& request, std::uint64_t request_id, carvera::sim::v1::Response& response,
                  std::chrono::milliseconds timeout = kStreamRequestTimeout) {
    return this->request(request, request_id, response, timeout) && response.ok();
  }

  template <typename Predicate>
  bool wait_frame(Predicate&& predicate, std::chrono::milliseconds timeout) {
    return wait_until(
        [&](const std::vector<carvera::sim::v1::StreamFrame>& frames) {
          for (const auto& frame : frames) {
            if (predicate(frame)) {
              return true;
            }
          }
          return false;
        },
        timeout);
  }

  template <typename Predicate>
  bool wait_until(Predicate&& predicate, std::chrono::milliseconds timeout) {
    std::unique_lock lock(frames_mutex_);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
      if (predicate(frames_)) {
        return true;
      }
      if (stream_closed_) {
        return false;
      }
      if (frames_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
        return false;
      }
    }
  }

  template <typename Predicate>
  bool any_frame(Predicate&& predicate) const {
    std::lock_guard lock(frames_mutex_);
    for (const auto& frame : frames_) {
      if (predicate(frame)) {
        return true;
      }
    }
    return false;
  }

  std::string stderr_output() {
    if (err_child_ >= 0) {
      stderr_output_ += read_all_nonblocking(err_child_);
    }
    return stderr_output_;
  }

 private:
  void read_loop() {
    for (;;) {
      carvera::sim::v1::StreamFrame frame;
      if (!read_framed_message(from_child_, frame)) {
        break;
      }
      {
        std::lock_guard lock(frames_mutex_);
        frames_.push_back(std::move(frame));
      }
      frames_cv_.notify_all();
    }
    {
      std::lock_guard lock(frames_mutex_);
      stream_closed_ = true;
    }
    frames_cv_.notify_all();
  }

  static std::string read_all(int fd) {
    std::string output;
    char buffer[1024];
    for (;;) {
      const auto received = ::read(fd, buffer, sizeof(buffer));
      if (received > 0) {
        output.append(buffer, static_cast<std::size_t>(received));
        continue;
      }
      if (received < 0 && errno == EINTR) {
        continue;
      }
      return output;
    }
  }

  static std::string read_all_nonblocking(int fd) {
    std::string output;
    char buffer[1024];
    for (;;) {
      const auto received = ::read(fd, buffer, sizeof(buffer));
      if (received > 0) {
        output.append(buffer, static_cast<std::size_t>(received));
        continue;
      }
      if (received < 0 && errno == EINTR) {
        continue;
      }
      if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return output;
      }
      return output;
    }
  }

  std::string executable_;
  pid_t child_{-1};
  int to_child_{-1};
  int from_child_{-1};
  int err_child_{-1};
  std::thread reader_;
  std::mutex write_mutex_;
  mutable std::mutex frames_mutex_;
  std::condition_variable frames_cv_;
  std::vector<carvera::sim::v1::StreamFrame> frames_;
  bool stream_closed_{false};
  std::string stderr_output_;
};

}  // namespace sim::test

#endif
