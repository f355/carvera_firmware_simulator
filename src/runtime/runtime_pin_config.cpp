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

#include "sim/runtime_pin_config.hpp"

#include "Config.h"
#include "ConfigValue.h"
#include "libs/Kernel.h"
#include "sim/board_profile.hpp"

namespace sim::runtime_pins {

PinAddress pin_address(const Pin& pin) { return PinAddress{static_cast<std::uint8_t>(pin.port_number), pin.pin}; }

bool raw_level_for_pin_get(const Pin& pin, bool logical_value) { return pin.is_inverting() ^ logical_value; }

const char* default_main_button_pin(MachineModel model) { return board_profile(model).default_main_button_pin; }

const char* default_e_stop_pin(MachineModel model) { return board_profile(model).default_e_stop_pin; }

const char* default_cover_pin(MachineModel model) { return board_profile(model).default_cover_pin; }

const char* default_main_button_led_r_pin(MachineModel model) {
  return board_profile(model).default_main_button_led_r_pin;
}

const char* default_main_button_led_g_pin(MachineModel model) {
  return board_profile(model).default_main_button_led_g_pin;
}

const char* default_main_button_led_b_pin(MachineModel model) {
  return board_profile(model).default_main_button_led_b_pin;
}

Pin configured_pin(Kernel& kernel, std::uint16_t checksum, const char* default_pin) {
  Pin pin;
  if (kernel.config == nullptr) {
    pin.from_string(default_pin);
    return pin;
  }
  pin.from_string(kernel.config->value(checksum)->as_string(default_pin));
  return pin;
}

Pin configured_pin(Kernel& kernel, std::uint16_t checksum_a, std::uint16_t checksum_b, const char* default_pin) {
  Pin pin;
  if (kernel.config == nullptr) {
    pin.from_string(default_pin);
    return pin;
  }
  pin.from_string(kernel.config->value(checksum_a, checksum_b)->as_string(default_pin));
  return pin;
}

void drive_configured_input(MachineSimulator& simulator, Kernel& kernel, std::uint16_t checksum,
                            const char* default_pin, bool logical_value) {
  Pin pin = configured_pin(kernel, checksum, default_pin);
  if (pin.connected()) {
    simulator.set_gpio_input(pin_address(pin), raw_level_for_pin_get(pin, logical_value));
  }
}

bool read_configured_input(Kernel& kernel, std::uint16_t checksum, const char* default_pin) {
  Pin pin = configured_pin(kernel, checksum, default_pin);
  return pin.connected() && pin.get();
}

bool read_configured_output(Kernel& kernel, std::uint16_t checksum, const char* default_pin, bool* available) {
  Pin pin = configured_pin(kernel, checksum, default_pin);
  if (!pin.connected()) {
    return false;
  }
  *available = true;
  return pin.get();
}

}  // namespace sim::runtime_pins
