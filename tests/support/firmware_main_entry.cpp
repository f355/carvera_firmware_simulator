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

#ifndef CARVERA_FIRMWARE_MAIN
#error "CARVERA_FIRMWARE_MAIN must name the upstream firmware main.cpp"
#endif

// main.cpp uses source-relative quoted includes for these embedded-only
// headers. Load the host implementations first and reserve the upstream include
// guards before including the real entrypoint.
#define MSCFILESYSTEM_H
#include "libs/USBDevice/MSCFileSystem.h"

#define SD_FILE_SYSTEM_H
#include "libs/USBDevice/SDCard/SDFileSystem.h"

#define _SDFAT_H
#include "libs/SDFAT.h"

#define COMPILER_H
#include "libs/compiler.h"

#include CARVERA_FIRMWARE_MAIN
