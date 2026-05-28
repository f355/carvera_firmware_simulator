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

#ifndef SIMULATOR_TESTS_SUPPORT_FRAMED_PROTO_CLIENT_HPP
#define SIMULATOR_TESTS_SUPPORT_FRAMED_PROTO_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "carvera_sim.pb.h"
#include "posix_io.hpp"

namespace sim::test {

inline std::uint32_t decode_frame_size(const char header[4]) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(header[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(header[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(header[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(header[3])) << 24);
}

template <typename Message>
bool write_framed_message(int fd, const Message& message) {
  std::string payload;
  if (!message.SerializeToString(&payload)) {
    return false;
  }

  const auto size = static_cast<std::uint32_t>(payload.size());
  const char header[4] = {
      static_cast<char>(size & 0xFF),
      static_cast<char>((size >> 8) & 0xFF),
      static_cast<char>((size >> 16) & 0xFF),
      static_cast<char>((size >> 24) & 0xFF),
  };

  return write_exact(fd, header, sizeof(header)) && write_exact(fd, payload.data(), payload.size());
}

template <typename Message>
bool read_framed_message(int fd, Message& message) {
  char header[4]{};
  if (!read_exact(fd, header, sizeof(header))) {
    return false;
  }

  const auto size = decode_frame_size(header);
  if (size > 16 * 1024 * 1024) {
    return false;
  }

  std::vector<char> payload(size);
  if (!read_exact(fd, payload.data(), payload.size())) {
    return false;
  }

  return message.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
}

inline bool read_stream_response(int fd, std::uint64_t request_id, carvera::sim::v1::Response& response) {
  for (;;) {
    carvera::sim::v1::StreamFrame frame;
    if (!read_framed_message(fd, frame)) {
      return false;
    }
    if (frame.payload_case() == carvera::sim::v1::StreamFrame::kResponse && frame.response().id() == request_id) {
      response = frame.response();
      return true;
    }
  }
}

inline bool read_stream_frame_timeout(int fd, carvera::sim::v1::StreamFrame& frame, std::chrono::milliseconds timeout) {
  char header[4]{};
  if (!read_exact_timeout(fd, header, sizeof(header), timeout)) {
    return false;
  }

  const auto size = decode_frame_size(header);
  if (size > 16 * 1024 * 1024) {
    return false;
  }

  std::vector<char> payload(size);
  if (!read_exact_timeout(fd, payload.data(), payload.size(), timeout)) {
    return false;
  }
  return frame.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
}

inline bool read_stream_response_timeout(int fd, std::uint64_t request_id, carvera::sim::v1::Response& response,
                                         std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return false;
    }

    carvera::sim::v1::StreamFrame frame;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    if (!read_stream_frame_timeout(fd, frame, remaining)) {
      return false;
    }
    if (frame.payload_case() == carvera::sim::v1::StreamFrame::kResponse && frame.response().id() == request_id) {
      response = frame.response();
      return true;
    }
  }
}

}  // namespace sim::test

#endif
