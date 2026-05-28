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

#include "sim/framed_proto.hpp"

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <string>

namespace {

constexpr std::uint32_t max_frame_size = 16 * 1024 * 1024;

std::array<char, 4> encode_size(std::uint32_t size) {
  return {
      static_cast<char>(size & 0xFF),
      static_cast<char>((size >> 8) & 0xFF),
      static_cast<char>((size >> 16) & 0xFF),
      static_cast<char>((size >> 24) & 0xFF),
  };
}

std::uint32_t decode_size(const std::array<char, 4>& bytes) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
}

}  // namespace

namespace sim::proto_framing {

bool write_message(std::ostream& stream, const google::protobuf::MessageLite& message) {
  const auto byte_size = message.ByteSizeLong();
  if (byte_size > std::numeric_limits<std::uint32_t>::max() || byte_size > max_frame_size) {
    return false;
  }

  std::string payload;
  if (!message.SerializeToString(&payload)) {
    return false;
  }

  const auto header = encode_size(static_cast<std::uint32_t>(payload.size()));
  stream.write(header.data(), static_cast<std::streamsize>(header.size()));
  stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  return stream.good();
}

bool read_message(std::istream& stream, google::protobuf::MessageLite& message) {
  std::array<char, 4> header{};
  stream.read(header.data(), static_cast<std::streamsize>(header.size()));
  if (!stream.good()) {
    return false;
  }

  const auto size = decode_size(header);
  if (size > max_frame_size) {
    return false;
  }

  std::string payload(size, '\0');
  stream.read(payload.data(), static_cast<std::streamsize>(payload.size()));
  if (!stream.good()) {
    return false;
  }

  return message.ParseFromString(payload);
}

}  // namespace sim::proto_framing
