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

#ifndef SIMULATOR_FATFILESYSTEM_H
#define SIMULATOR_FATFILESYSTEM_H

// The firmware's ChaNFS FATFileSystem.h drags in FatFs and mbed internals the
// host build has no use for; firmware modules include it only for the path
// budget its callers size buffers against. Mirror just that.
//
// The value must match the firmware header: SimpleShell's file-integrity
// commands reject paths against it, and the simulator only reproduces that
// behaviour if the limits agree. CMake fails the configure step when the two
// drift apart.
#ifndef FATFS_PATH_MAX
#define FATFS_PATH_MAX 260
#endif

#endif
