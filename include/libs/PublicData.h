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

#ifndef SIMULATOR_LIBS_PUBLICDATA_H
#define SIMULATOR_LIBS_PUBLICDATA_H

#include <cstdint>

class PublicData {
 public:
  static bool get_value(uint16_t csa, void* data) { return get_value(csa, 0, 0, data); }
  static bool get_value(uint16_t csa, uint16_t csb, void* data) { return get_value(csa, csb, 0, data); }
  static bool get_value(uint16_t cs[3], void* data) { return get_value(cs[0], cs[1], cs[2], data); }
  static bool get_value(uint16_t csa, uint16_t csb, uint16_t csc, void* data);

  static bool set_value(uint16_t csa, void* data) { return set_value(csa, 0, 0, data); }
  static bool set_value(uint16_t csa, uint16_t csb, void* data) { return set_value(csa, csb, 0, data); }
  static bool set_value(uint16_t cs[3], void* data) { return set_value(cs[0], cs[1], cs[2], data); }
  static bool set_value(uint16_t csa, uint16_t csb, uint16_t csc, void* data);
};

#endif
