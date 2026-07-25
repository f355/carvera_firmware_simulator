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

#ifndef SIMULATOR_SIM_FRAMED_PROTO_HPP
#define SIMULATOR_SIM_FRAMED_PROTO_HPP

#include <google/protobuf/message_lite.h>

#include <array>
#include <cstdint>
#include <iosfwd>

namespace sim::proto_framing {

inline constexpr std::uint32_t kMaxFrameSize = 16 * 1024 * 1024;

inline std::array<char, 4> encode_frame_size(std::uint32_t size) {
  return {
      static_cast<char>(size & 0xFF),
      static_cast<char>((size >> 8) & 0xFF),
      static_cast<char>((size >> 16) & 0xFF),
      static_cast<char>((size >> 24) & 0xFF),
  };
}

inline std::uint32_t decode_frame_size(const void* bytes) {
  const auto* octets = static_cast<const unsigned char*>(bytes);
  return static_cast<std::uint32_t>(octets[0]) | (static_cast<std::uint32_t>(octets[1]) << 8) |
         (static_cast<std::uint32_t>(octets[2]) << 16) | (static_cast<std::uint32_t>(octets[3]) << 24);
}

bool write_message(std::ostream& stream, const google::protobuf::MessageLite& message);
bool read_message(std::istream& stream, google::protobuf::MessageLite& message);

}  // namespace sim::proto_framing

#endif
