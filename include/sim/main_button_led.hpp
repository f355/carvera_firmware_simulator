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

#ifndef SIMULATOR_SIM_MAIN_BUTTON_LED_HPP
#define SIMULATOR_SIM_MAIN_BUTTON_LED_HPP

#include <array>
#include <cstdint>

namespace sim {

namespace main_button_led {

struct RgbColor {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};
};

using LedStrip = std::array<RgbColor, 5>;

}  // namespace main_button_led

class MainButtonLedState {
 public:
  void reset();
  void set_strip(const main_button_led::LedStrip& strip);
  bool available() const { return strip_available_; }
  main_button_led::LedStrip strip() const { return current_strip_; }

 private:
  bool strip_available_{false};
  main_button_led::LedStrip current_strip_{};
};

namespace main_button_led {

MainButtonLedState& active();
void reset();
void set_strip(const LedStrip& strip);
bool available();
LedStrip strip();

}  // namespace main_button_led

}  // namespace sim

#endif
