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

#include "sim/main_button_led.hpp"

#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace sim {

void MainButtonLedState::reset() {
  strip_available_ = false;
  current_strip_ = {};
}

void MainButtonLedState::set_strip(const main_button_led::LedStrip& strip) {
  strip_available_ = true;
  current_strip_ = strip;
}

namespace main_button_led {

MainButtonLedState& active() { return compat::active_context().main_button_led(); }

void reset() { active().reset(); }

void set_strip(const LedStrip& strip) { active().set_strip(strip); }

bool available() { return active().available(); }

LedStrip strip() { return active().strip(); }

}  // namespace main_button_led

}  // namespace sim
