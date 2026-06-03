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

#ifndef SIMULATOR_SIM_FILEPOS_COMPAT_H
#define SIMULATOR_SIM_FILEPOS_COMPAT_H

#include <cstdio>
#include <stdio.h>

static inline int sim_fgetpos_as_long(FILE* stream, long* pos) {
  long value = ftell(stream);
  if (value < 0) {
    return -1;
  }
  *pos = value;
  return 0;
}

static inline int sim_fsetpos_as_long(FILE* stream, const long* pos) { return fseek(stream, *pos, SEEK_SET); }

#define fpos_t long
#define fgetpos sim_fgetpos_as_long
#define fsetpos sim_fsetpos_as_long

#endif
