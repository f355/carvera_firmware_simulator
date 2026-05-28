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

#ifndef SIMULATOR_CHECKSUMM_H
#define SIMULATOR_CHECKSUMM_H

#include <stdint.h>

constexpr uint16_t sim_constexpr_checksum(const char* text) {
  uint16_t hash = 5381;
  while (*text != '\0') {
    hash = static_cast<uint16_t>(((hash << 5) + hash) ^ static_cast<unsigned char>(*text++));
  }
  return hash;
}

#define CHECKSUM(value) (sim_constexpr_checksum(value))

inline uint16_t get_checksum(const char* value) { return sim_constexpr_checksum(value); }

#endif
