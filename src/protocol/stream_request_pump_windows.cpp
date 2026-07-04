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

#include "sim/stream_request_pump.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <io.h>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <utility>
#include <windows.h>

#include "sim/framed_proto.hpp"

namespace {

constexpr std::uint32_t max_frame_size = 16 * 1024 * 1024;

std::uint32_t decode_frame_size(const std::array<unsigned char, 4>& bytes) {
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

enum class ReadResult {
  success,
  closed,
  stopped,
  failed,
};

ReadResult read_exact(HANDLE input, void* destination, std::size_t size, const std::atomic_bool& stopping) {
  auto* bytes = static_cast<unsigned char*>(destination);
  std::size_t offset = 0;
  while (offset < size) {
    if (stopping.load()) {
      return ReadResult::stopped;
    }
    DWORD count = 0;
    const auto remaining = static_cast<DWORD>(size - offset);
    if (::ReadFile(input, bytes + offset, remaining, &count, nullptr) == 0) {
      const DWORD error = ::GetLastError();
      if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
        return ReadResult::closed;
      }
      if (error == ERROR_OPERATION_ABORTED && stopping.load()) {
        return ReadResult::stopped;
      }
      return ReadResult::failed;
    }
    if (count == 0) {
      return ReadResult::closed;
    }
    offset += count;
  }
  return ReadResult::success;
}

}  // namespace

namespace sim {

struct StreamRequestPump::Impl {
  explicit Impl(std::ostream& output) : output_(output), input_(::GetStdHandle(STD_INPUT_HANDLE)) {
    if (_setmode(_fileno(stdout), _O_BINARY) == -1 || input_ == nullptr || input_ == INVALID_HANDLE_VALUE) {
      failed_ = true;
      return;
    }
    reader_ = std::thread([this] { read_requests(); });
  }

  ~Impl() {
    stopping_.store(true);
    if (!reader_.joinable()) {
      return;
    }

    const HANDLE thread = reinterpret_cast<HANDLE>(reader_.native_handle());
    while (::WaitForSingleObject(thread, 0) == WAIT_TIMEOUT) {
      ::CancelSynchronousIo(thread);
      ::WaitForSingleObject(thread, 50);
    }
    reader_.join();
  }

  bool drain_available(const Handler& handler) {
    std::deque<carvera::sim::v1::Request> requests;
    bool failed = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      requests.swap(requests_);
      failed = failed_;
    }

    for (const auto& request : requests) {
      carvera::sim::v1::StreamFrame frame;
      frame.mutable_response()->CopyFrom(handler(request));
      if (!proto_framing::write_message(output_, frame)) {
        return false;
      }
      output_.flush();
      if (!output_.good()) {
        return false;
      }
    }
    return !failed;
  }

  void read_requests() {
    for (;;) {
      std::array<unsigned char, 4> header{};
      const auto header_result = read_exact(input_, header.data(), header.size(), stopping_);
      if (header_result != ReadResult::success) {
        finish(header_result);
        return;
      }

      const auto size = decode_frame_size(header);
      if (size > max_frame_size) {
        finish(ReadResult::failed);
        return;
      }

      std::string payload(size, '\0');
      const auto payload_result = read_exact(input_, payload.data(), payload.size(), stopping_);
      if (payload_result != ReadResult::success) {
        finish(payload_result);
        return;
      }

      carvera::sim::v1::Request request;
      if (!request.ParseFromString(payload)) {
        finish(ReadResult::failed);
        return;
      }

      std::lock_guard<std::mutex> lock(mutex_);
      requests_.push_back(std::move(request));
    }
  }

  void finish(ReadResult result) {
    if (result == ReadResult::stopped) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = result == ReadResult::closed;
    failed_ = result == ReadResult::failed;
  }

  std::ostream& output_;
  HANDLE input_;
  std::atomic_bool stopping_{false};
  std::thread reader_;
  mutable std::mutex mutex_;
  std::deque<carvera::sim::v1::Request> requests_;
  bool closed_{false};
  bool failed_{false};
};

StreamRequestPump::StreamRequestPump(std::ostream& output) : impl_(std::make_unique<Impl>(output)) {}

StreamRequestPump::~StreamRequestPump() = default;

bool StreamRequestPump::closed() const {
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  return impl_->closed_;
}

bool StreamRequestPump::drain_available(const Handler& handler) { return impl_->drain_available(handler); }

}  // namespace sim
