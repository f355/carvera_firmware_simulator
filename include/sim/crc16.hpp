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

#ifndef SIMULATOR_SIM_CRC16_HPP
#define SIMULATOR_SIM_CRC16_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sim {

// CRC16-CCITT (polynomial 0x1021, zero seed), as used by the Makera serial
// protocol and the EEPROM factory page checksum.
inline std::uint16_t crc16_ccitt(const void* data, std::size_t len) {
  const auto* bytes = static_cast<const unsigned char*>(data);
  std::uint16_t crc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[i]) << 8);
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
  }
  return crc;
}

inline std::uint16_t crc16_ccitt(std::string_view bytes) { return crc16_ccitt(bytes.data(), bytes.size()); }

}  // namespace sim

#endif
