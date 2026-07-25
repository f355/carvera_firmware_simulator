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

namespace sim::proto_framing {

bool write_message(std::ostream& stream, const google::protobuf::MessageLite& message) {
  const auto byte_size = message.ByteSizeLong();
  if (byte_size > std::numeric_limits<std::uint32_t>::max() || byte_size > kMaxFrameSize) {
    return false;
  }

  std::string payload;
  if (!message.SerializeToString(&payload)) {
    return false;
  }

  const auto header = encode_frame_size(static_cast<std::uint32_t>(payload.size()));
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

  const auto size = decode_frame_size(header.data());
  if (size > kMaxFrameSize) {
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
