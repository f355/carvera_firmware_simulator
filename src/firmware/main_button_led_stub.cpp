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

void mainbutton_led_write_strip(unsigned char R1, unsigned char G1, unsigned char B1, unsigned char R2,
                                unsigned char G2, unsigned char B2, unsigned char R3, unsigned char G3,
                                unsigned char B3, unsigned char R4, unsigned char G4, unsigned char B4,
                                unsigned char R5, unsigned char G5, unsigned char B5) {
  sim::main_button_led::set_strip({{{R1, G1, B1}, {R2, G2, B2}, {R3, G3, B3}, {R4, G4, B4}, {R5, G5, B5}}});
}
