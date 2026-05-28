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

#include "sim/physical_tooling.hpp"

#include <algorithm>
#include <cmath>

namespace sim::physical_tooling {
namespace {

constexpr double stock_z_probe_tip_diameter_mm = 1.6;
constexpr double three_axis_probe_tip_diameter_mm = 2.0;

}  // namespace

bool close_enough(double lhs, double rhs, double tolerance) { return std::abs(lhs - rhs) <= tolerance; }

bool is_firmware_probe_tool(int tool) {
  // Mirrors ZProbe::check_probe_tool() and ATCHandler calibration paths.
  // T0 is the stock wireless/Z probe; T999990+ are firmware custom/3-axis probes.
  return tool == 0 || tool >= 999990;
}

ToolKind inferred_tool_kind(int tool) {
  if (tool >= 999990) {
    return ToolKind::ThreeAxisProbe;
  }
  if (tool == 0) {
    return ToolKind::StockZProbe;
  }
  return ToolKind::CuttingTool;
}

ToolKind resolve_tool_kind(int tool, ToolKind requested) {
  return requested == ToolKind::Unspecified ? inferred_tool_kind(tool) : requested;
}

double resolve_probe_tip_diameter(int tool, ToolKind kind, double requested) {
  if (requested > 0.0) {
    return requested;
  }
  switch (kind) {
    case ToolKind::ThreeAxisProbe:
      return three_axis_probe_tip_diameter_mm;
    case ToolKind::StockZProbe:
      return stock_z_probe_tip_diameter_mm;
    case ToolKind::Unspecified:
      return resolve_probe_tip_diameter(tool, inferred_tool_kind(tool), 0.0);
    case ToolKind::CuttingTool:
    default:
      return 0.0;
  }
}

bool is_probe_kind(ToolKind kind) { return kind == ToolKind::StockZProbe || kind == ToolKind::ThreeAxisProbe; }

Point3 tool_tip_position(Point3 spindle_position, double tool_length_mm) {
  const double cutting_stickout = std::max(0.0, tool_length_mm - tool_shank_insert_mm);
  spindle_position.z -= cutting_stickout;
  return spindle_position;
}

Box normalized(Box box) {
  if (box.min_x > box.max_x) {
    std::swap(box.min_x, box.max_x);
  }
  if (box.min_y > box.max_y) {
    std::swap(box.min_y, box.max_y);
  }
  if (box.min_z > box.max_z) {
    std::swap(box.min_z, box.max_z);
  }
  return box;
}

bool contains(const Box& box, Point3 point) {
  return point.x >= box.min_x && point.x <= box.max_x && point.y >= box.min_y && point.y <= box.max_y &&
         point.z >= box.min_z && point.z <= box.max_z;
}

}  // namespace sim::physical_tooling
