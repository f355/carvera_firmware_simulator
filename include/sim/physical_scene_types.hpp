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

#ifndef SIMULATOR_SIM_PHYSICAL_SCENE_TYPES_HPP
#define SIMULATOR_SIM_PHYSICAL_SCENE_TYPES_HPP

#include <cstddef>
#include <optional>

#include "sim/i2c_eeprom.hpp"

namespace sim {

struct Point3 {
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

struct Box {
  double min_x{0.0};
  double min_y{0.0};
  double min_z{0.0};
  double max_x{0.0};
  double max_y{0.0};
  double max_z{0.0};
};

enum class ToolKind {
  Unspecified,
  CuttingTool,
  StockZProbe,
  ThreeAxisProbe,
};

struct AtcPocketState {
  int pocket{0};
  int tool{0};
  bool occupied{false};
  double length_mm{0.0};
  ToolKind kind{ToolKind::Unspecified};
  double probe_tip_diameter_mm{0.0};
  Point3 position{};
};

struct AtcSpindleState {
  int tool{-1};
  double length_mm{0.0};
  ToolKind kind{ToolKind::Unspecified};
  double probe_tip_diameter_mm{0.0};
  bool has_tool{false};
};

struct PhysicalAtcConfig {
  MachineModel model{MachineModel::CarveraC1};
  bool atc_available{false};
  std::optional<Box> tool_setter_box{};
  double anchor1_x{-359.0};
  double anchor1_y{-234.0};
  double toolrack_offset_x{356.0};
  double toolrack_offset_y{0.0};
  double toolrack_z{-105.0};
  double clamp_action_mm{1.0};
  std::optional<int> persisted_active_tool{};
};

struct ProbeContactState {
  bool probe_contact{false};
  bool tool_setter_contact{false};
  std::optional<std::size_t> stock_probe_crash_axis{};
};

}  // namespace sim

#endif
