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

#ifndef SIMULATOR_SIM_RUNTIME_PHYSICAL_TYPES_HPP
#define SIMULATOR_SIM_RUNTIME_PHYSICAL_TYPES_HPP

#include <array>
#include <cstdint>

#include "sim/machine_simulator.hpp"
#include "sim/physical_scene_types.hpp"

namespace sim {

struct RgbLedState {
  bool available{false};
  bool red{false};
  bool green{false};
  bool blue{false};
};

struct RgbColorState {
  std::uint8_t red{0};
  std::uint8_t green{0};
  std::uint8_t blue{0};
};

struct LedStripState {
  bool available{false};
  std::array<RgbColorState, 5> segments{};
};

struct FrontPanelState {
  bool main_button_pressed{false};
  bool e_stop_pressed{false};
  PowerRailState power_rails{};
  RgbLedState direct_rgb{};
  LedStripState led_strip{};
};

enum class LimitSwitchSide {
  Min,
  Max,
};

enum class MachineSwitch {
  Light,
  Vacuum,
  SpindleFan,
  PowerFan,
  ToolSensor,
  ProbeCharger,
  Air,
  Beep,
};

struct SwitchState {
  bool available{false};
  bool on{false};
  float value{0.0F};
};

struct LaserState {
  bool available{false};
  bool mode{false};
  bool firing{false};
  bool testing{false};
  float power_percent{0.0F};
  float scale_percent{0.0F};
};

}  // namespace sim

#endif
