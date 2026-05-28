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

#ifndef SIMULATOR_SIM_RUNTIME_PIN_CONFIG_HPP
#define SIMULATOR_SIM_RUNTIME_PIN_CONFIG_HPP

#include <cstdint>

#include "Pin.h"
#include "sim/machine_simulator.hpp"

class Kernel;

namespace sim::runtime_pins {

PinAddress pin_address(const Pin& pin);
bool raw_level_for_pin_get(const Pin& pin, bool logical_value);
const char* default_main_button_pin(MachineModel model);
const char* default_e_stop_pin(MachineModel model);
const char* default_cover_pin(MachineModel model);
const char* default_main_button_led_r_pin(MachineModel model);
const char* default_main_button_led_g_pin(MachineModel model);
const char* default_main_button_led_b_pin(MachineModel model);
Pin configured_pin(Kernel& kernel, std::uint16_t checksum, const char* default_pin);
Pin configured_pin(Kernel& kernel, std::uint16_t checksum_a, std::uint16_t checksum_b, const char* default_pin);
void drive_configured_input(MachineSimulator& simulator, Kernel& kernel, std::uint16_t checksum,
                            const char* default_pin, bool logical_value);
bool read_configured_input(Kernel& kernel, std::uint16_t checksum, const char* default_pin);
bool read_configured_output(Kernel& kernel, std::uint16_t checksum, const char* default_pin, bool* available);

}  // namespace sim::runtime_pins

#endif
