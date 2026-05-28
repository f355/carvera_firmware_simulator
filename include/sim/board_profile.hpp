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

#ifndef SIMULATOR_SIM_BOARD_PROFILE_HPP
#define SIMULATOR_SIM_BOARD_PROFILE_HPP

#include <array>
#include <optional>

#include "sim/i2c_eeprom.hpp"
#include "sim/machine_geometry.hpp"
#include "sim/pin_address.hpp"

namespace sim {

struct BoardSignal {
  PinAddress pin{};
  bool active_level{true};
  bool connected{false};
};

struct AxisLimitSignals {
  BoardSignal min{};
  BoardSignal max{};
};

struct BoardProfile {
  MachineModel model{MachineModel::CarveraC1};
  const char* name{""};
  const char* default_main_button_pin{""};
  const char* default_e_stop_pin{""};
  const char* default_cover_pin{""};
  const char* default_main_button_led_r_pin{""};
  const char* default_main_button_led_g_pin{""};
  const char* default_main_button_led_b_pin{""};
  BoardSignal physical_main_button{};
  BoardSignal physical_e_stop{};
  BoardSignal physical_probe{};
  BoardSignal physical_tool_setter{};
  BoardSignal physical_atc_detector{};
  BoardSignal physical_atc_home{};
  BoardSignal spindle_fan_output{};
  BoardSignal power_fan_output{};
  std::array<AxisLimitSignals, 3> physical_limits{};
  std::optional<MachineGeometry> geometry{};
};

const BoardProfile& board_profile(MachineModel model);
bool signal_level(const BoardSignal& signal, bool active);

}  // namespace sim

#endif
