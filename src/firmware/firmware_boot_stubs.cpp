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

#include "sim/firmware_boot_stubs.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "Gcode.h"
#include "Robot.h"
#include "SerialMessage.h"
#include "StreamOutput.h"
#include "gpio.h"
#include "libs/Kernel.h"

int BootModule::loaded_count = 0;
int SDFileSystem::disk_status = 1;
SimMemoryPool simulator_ahb;
#ifdef _WIN32
#define SIM_WEAK_FIRMWARE_GLOBAL
#else
#define SIM_WEAK_FIRMWARE_GLOBAL __attribute__((weak))
#endif
SDFAT mounter SIM_WEAK_FIRMWARE_GLOBAL ("sd", nullptr);
GPIO leds[4] SIM_WEAK_FIRMWARE_GLOBAL = {
    GPIO(P4_29),
    GPIO(P4_28),
    GPIO(P0_4),
    GPIO(P1_17),
};
#undef SIM_WEAK_FIRMWARE_GLOBAL

namespace sim::boot {

void reset_boot_stubs() {
  BootModule::reset_counts();
  SDFileSystem::disk_status = 1;
}

int loaded_module_count() { return BootModule::loaded_count; }

}  // namespace sim::boot
