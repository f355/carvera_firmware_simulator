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

#include "MainButtonLed.h"

#include "sim/main_button_led.hpp"

void MainButtonLed::set_pin(const Pin&) {}

void MainButtonLed::set_all(Color color) const {
  Colors colors;
  colors.fill(color);
  write(colors, false);
}

void MainButtonLed::set_number(Color front, Color back, uint8_t number, bool row) const {
  const uint8_t maximum = row ? 5 : 3;
  if (number == 0 || number > maximum) {
    return;
  }

  Colors colors;
  colors.fill(back);
  for (uint8_t index = 0; index < colors.size(); ++index) {
    if (row ? index < number : index % 2 == 0 && index / 2 < number) {
      colors[index] = front;
    }
  }
  write(colors, true);
}

void MainButtonLed::write(const Colors& colors, bool) const {
  sim::main_button_led::LedStrip strip;
  for (std::size_t index = 0; index < colors.size(); ++index) {
    strip[index] = {colors[index].red, colors[index].green, colors[index].blue};
  }
  sim::main_button_led::set_strip(strip);
}
