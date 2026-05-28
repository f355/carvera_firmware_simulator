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

#ifndef SIMULATOR_SIM_PHYSICAL_TOOLING_HPP
#define SIMULATOR_SIM_PHYSICAL_TOOLING_HPP

#include "sim/physical_scene_types.hpp"

namespace sim::physical_tooling {

inline constexpr double tool_shank_insert_mm = 20.0;
inline constexpr double tool_shank_radius_mm = 3.0;
inline constexpr double loose_collet_clearance_mm = 0.8;
inline constexpr double detector_tolerance_mm = tool_shank_radius_mm + loose_collet_clearance_mm;

bool close_enough(double lhs, double rhs, double tolerance);
bool is_firmware_probe_tool(int tool);
ToolKind inferred_tool_kind(int tool);
ToolKind resolve_tool_kind(int tool, ToolKind requested);
double resolve_probe_tip_diameter(int tool, ToolKind kind, double requested);
bool is_probe_kind(ToolKind kind);
Point3 tool_tip_position(Point3 spindle_position, double tool_length_mm);
Box normalized(Box box);
bool contains(const Box& box, Point3 point);

}  // namespace sim::physical_tooling

#endif
