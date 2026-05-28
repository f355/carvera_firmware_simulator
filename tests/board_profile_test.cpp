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

#include <string>

#include "sim/board_profile.hpp"
#include "support/assertions.hpp"

int main() {
  using sim::test::require;

  const auto& c1 = sim::board_profile(sim::MachineModel::CarveraC1);
  require(c1.default_main_button_pin == std::string("1.16^"), "C1 main-button config default should be inventoried");
  require(c1.default_e_stop_pin == std::string("0.26^"), "C1 e-stop config default should be inventoried");
  require(c1.default_cover_pin == std::string("1.9^"), "C1 cover config default should be inventoried");
  require(c1.default_main_button_led_r_pin == std::string("1.10"), "C1 red LED default should be inventoried");
  require(c1.default_main_button_led_g_pin == std::string("1.15"), "C1 green LED default should be inventoried");
  require(c1.default_main_button_led_b_pin == std::string("1.14"), "C1 blue LED default should be inventoried");
  require(c1.physical_main_button.pin.port == 1 && c1.physical_main_button.pin.pin == 16,
          "C1 main button should map to the physical board pin");
  require(sim::signal_level(c1.physical_e_stop, true) == false, "C1 physical e-stop is active-low");
  require(c1.physical_probe.pin.port == 2 && c1.physical_probe.pin.pin == 6, "C1 probe signal should be inventoried");
  require(c1.physical_tool_setter.pin.port == 0 && c1.physical_tool_setter.pin.pin == 5,
          "C1 tool-setter signal should be inventoried");
  require(c1.physical_atc_detector.pin.port == 0 && c1.physical_atc_detector.pin.pin == 20,
          "C1 ATC detector signal should be inventoried");
  require(c1.physical_atc_home.pin.port == 1 && c1.physical_atc_home.pin.pin == 0,
          "C1 ATC home signal should be inventoried");
  require(c1.spindle_fan_output.pin.port == 2 && c1.spindle_fan_output.pin.pin == 1,
          "C1 spindle fan output should be inventoried");
  require(c1.physical_limits[0].min.pin.port == 0 && c1.physical_limits[0].min.pin.pin == 24,
          "C1 X min limit should be inventoried");
  require(c1.physical_limits[0].max.pin.port == 0 && c1.physical_limits[0].max.pin.pin == 25,
          "C1 X max limit should be inventoried");
  require(!c1.physical_limits[2].min.connected, "C1 does not have a physical Z min limit switch");

  const auto& ca1 = sim::board_profile(sim::MachineModel::CarveraAirCA1);
  require(ca1.default_main_button_pin == std::string("2.13!^"), "CA1 main-button config default should be inventoried");
  require(ca1.default_e_stop_pin == std::string("0.20^"), "CA1 e-stop config default should be inventoried");
  require(ca1.default_cover_pin == std::string("1.8!^"), "CA1 cover config default should be inventoried");
  require(ca1.default_main_button_led_r_pin == std::string("nc"), "CA1 red direct-LED default should be unavailable");
  require(ca1.default_main_button_led_g_pin == std::string("1.15"),
          "CA1 green direct-LED default should be inventoried");
  require(ca1.default_main_button_led_b_pin == std::string("nc"), "CA1 blue direct-LED default should be unavailable");
  require(ca1.physical_main_button.pin.port == 2 && ca1.physical_main_button.pin.pin == 13,
          "CA1 main button should map to the physical board pin");
  require(sim::signal_level(ca1.physical_main_button, true) == false, "CA1 physical main button is active-low");
  require(ca1.power_fan_output.pin.port == 2 && ca1.power_fan_output.pin.pin == 3,
          "CA1 power fan output should be inventoried");
  require(!ca1.physical_limits[0].min.connected, "CA1 does not have a physical X min limit switch");
  require(!ca1.physical_limits[1].min.connected, "CA1 does not have a physical Y min limit switch");
  require(!ca1.physical_limits[2].min.connected, "CA1 does not have a physical Z min limit switch");
  require(ca1.physical_limits[0].max.pin.port == 0 && ca1.physical_limits[0].max.pin.pin == 24,
          "CA1 X max limit should be inventoried");
  require(ca1.physical_limits[1].max.pin.port == 0 && ca1.physical_limits[1].max.pin.pin == 25,
          "CA1 Y max limit should be inventoried");
  require(ca1.physical_limits[2].max.pin.port == 1 && ca1.physical_limits[2].max.pin.pin == 1,
          "CA1 Z max limit should be inventoried");
  require(ca1.geometry.has_value(), "CA1 machine geometry should live with the board profile");
  require(ca1.geometry->physical_travel.min_x == -303.0, "CA1 physical travel should be inventoried");
  require(ca1.geometry->tool_setter.max_z == -115.0, "CA1 ETS trigger point should match the CAD model top");
  return 0;
}
