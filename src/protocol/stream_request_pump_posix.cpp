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
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <ostream>
#include <poll.h>
#include <string>
#include <unistd.h>

#include "sim/framed_proto.hpp"

namespace {

constexpr std::uint32_t max_frame_size = 16 * 1024 * 1024;

std::uint32_t decode_frame_size(const std::string& buffer) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[3])) << 24);
}

}  // namespace

namespace sim {

struct StreamRequestPump::Impl {
  explicit Impl(std::ostream& output) : output_(output) {
    const int flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
      ::fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
  }

  bool drain_available(const Handler& handler) {
    if (!read_available()) {
      return false;
    }

    while (buffer_.size() >= 4) {
      const auto size = decode_frame_size(buffer_);
      if (size > max_frame_size) {
        return false;
      }
      if (buffer_.size() < 4 + size) {
        break;
      }

      carvera::sim::v1::Request request;
      if (!request.ParseFromArray(buffer_.data() + 4, static_cast<int>(size))) {
        return false;
      }
      buffer_.erase(0, 4 + size);

      carvera::sim::v1::StreamFrame frame;
      frame.mutable_response()->CopyFrom(handler(request));
      if (!proto_framing::write_message(output_, frame)) {
        return false;
      }
      output_.flush();
    }
    return true;
  }

  bool read_available() {
    pollfd fd{};
    fd.fd = STDIN_FILENO;
    fd.events = POLLIN;
    bool read_any = false;
    for (;;) {
      const int ready = ::poll(&fd, 1, 0);
      if (ready == 0) {
        return true;
      }
      if (ready < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if ((fd.revents & (POLLHUP | POLLERR)) != 0 && (fd.revents & POLLIN) == 0) {
        closed_ = true;
        return false;
      }
      if ((fd.revents & POLLIN) == 0) {
        return true;
      }

      std::array<char, 4096> chunk{};
      const auto count = ::read(STDIN_FILENO, chunk.data(), chunk.size());
      if (count > 0) {
        read_any = true;
        buffer_.append(chunk.data(), static_cast<std::size_t>(count));
        continue;
      }
      if (count == 0) {
        closed_ = true;
        return false;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return true;
      }
      return false;
    }
    return read_any;
  }

  std::ostream& output_;
  bool closed_{false};
  std::string buffer_;
};

StreamRequestPump::StreamRequestPump(std::ostream& output) : impl_(std::make_unique<Impl>(output)) {}

StreamRequestPump::~StreamRequestPump() = default;

bool StreamRequestPump::closed() const { return impl_->closed_; }

bool StreamRequestPump::drain_available(const Handler& handler) { return impl_->drain_available(handler); }

}  // namespace sim
