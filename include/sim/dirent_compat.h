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

#ifndef SIMULATOR_DIRENT_COMPAT_H
#define SIMULATOR_DIRENT_COMPAT_H

#include <dirent.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

// Smoothie/FatFs dirent exposes metadata that host POSIX dirent does not.
// SimpleShell only needs this for decorated listings, so provide harmless
// member-name aliases for the host build.
#ifdef _WIN32
#ifndef NAME_MAX
#define NAME_MAX 260
#endif
#define d_isdir d_reclen != 0
#else
#define d_isdir d_type == DT_DIR
#endif
#define d_fsize d_reclen
#define d_date d_reclen
#define d_time d_reclen

#endif
