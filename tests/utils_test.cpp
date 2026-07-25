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

#include <cstring>

#include "checksumm.h"
#include "support/assertions.hpp"
#include "utils.h"

int main() {
  sim::test::require(CHECKSUM("") == 0, "The empty string should have a zero Fletcher checksum");
  sim::test::require(CHECKSUM("alpha") == 0x1c08, "Compile-time checksums should match firmware Fletcher checksums");
  sim::test::require(get_checksum("alpha") == 0x1c08,
                     "Runtime checksums should match firmware Fletcher checksums");
  sim::test::require(get_checksum("sd_ok") == CHECKSUM("sd_ok"),
                     "Runtime and compile-time checksums should agree");

  sim::test::require(std::strcmp(ltrim_cstr(" \t\r\nG1 X1"), "G1 X1") == 0,
                     "ltrim_cstr should advance past leading whitespace");
  sim::test::require(std::strcmp(ltrim_cstr(""), "") == 0, "ltrim_cstr should accept an empty string");

  const char* unchanged = "G1 X1";
  sim::test::require(ltrim_cstr(unchanged) == unchanged,
                     "ltrim_cstr should preserve a string that has no leading whitespace");
  sim::test::require(std::strcmp(ltrim_cstr(" \t\r\n\f\v"), "") == 0,
                     "ltrim_cstr should consume every C whitespace character");

  return 0;
}
