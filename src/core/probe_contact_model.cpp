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

#include "sim/probe_contact_model.hpp"

#include <cmath>

#include "sim/physical_tooling.hpp"

namespace sim {
namespace {

constexpr double z_probe_top_contact_window_mm = 0.25;
constexpr std::size_t x_axis = 0;
constexpr std::size_t y_axis = 1;

bool tool_tip_contacts_box(const Box& box, Point3 spindle_position, double tool_length_mm) {
  const Point3 tip = physical_tooling::tool_tip_position(spindle_position, tool_length_mm);
  return tip.x >= box.min_x && tip.x <= box.max_x && tip.y >= box.min_y && tip.y <= box.max_y && tip.z <= box.max_z;
}

bool stock_z_probe_contacts_top_face(const Box& box, Point3 spindle_position, double tool_length_mm) {
  const Point3 tip = physical_tooling::tool_tip_position(spindle_position, tool_length_mm);
  return tip.x >= box.min_x && tip.x <= box.max_x && tip.y >= box.min_y && tip.y <= box.max_y && tip.z <= box.max_z &&
         tip.z >= box.max_z - z_probe_top_contact_window_mm;
}

std::optional<std::size_t> stock_z_probe_side_crash_axis(const Box& box, Point3 previous_spindle_position,
                                                         Point3 spindle_position, double tool_length_mm) {
  const Point3 tip = physical_tooling::tool_tip_position(spindle_position, tool_length_mm);
  const bool inside_stock = tip.x >= box.min_x && tip.x <= box.max_x && tip.y >= box.min_y && tip.y <= box.max_y &&
                            tip.z >= box.min_z && tip.z <= box.max_z;
  if (!inside_stock || stock_z_probe_contacts_top_face(box, spindle_position, tool_length_mm)) {
    return std::nullopt;
  }

  const double dx = std::abs(spindle_position.x - previous_spindle_position.x);
  const double dy = std::abs(spindle_position.y - previous_spindle_position.y);
  const double dz = std::abs(spindle_position.z - previous_spindle_position.z);
  if (dx >= dy && dx > dz) {
    return x_axis;
  }
  if (dy > dx && dy > dz) {
    return y_axis;
  }
  return std::nullopt;
}

double distance_outside_axis(double value, double min, double max) {
  if (value < min) {
    return min - value;
  }
  if (value > max) {
    return value - max;
  }
  return 0.0;
}

bool sphere_contacts_box(const Box& box, Point3 center, double radius) {
  const double dx = distance_outside_axis(center.x, box.min_x, box.max_x);
  const double dy = distance_outside_axis(center.y, box.min_y, box.max_y);
  const double dz = distance_outside_axis(center.z, box.min_z, box.max_z);
  return dx * dx + dy * dy + dz * dz <= radius * radius;
}

bool probe_tool_contacts_box(const Box& box, Point3 spindle_position, const AtcSpindleState& tool) {
  const Point3 tip = physical_tooling::tool_tip_position(spindle_position, tool.length_mm);
  if (tool.kind == ToolKind::ThreeAxisProbe) {
    return sphere_contacts_box(box, tip, tool.probe_tip_diameter_mm / 2.0) ||
           tool_tip_contacts_box(box, spindle_position, tool.length_mm);
  }
  if (tool.kind == ToolKind::StockZProbe) {
    return stock_z_probe_contacts_top_face(box, spindle_position, tool.length_mm);
  }
  return false;
}

}  // namespace

void ProbeContactModel::clear() {
  probe_tool_installed_ = false;
  tool_setter_box_.reset();
  stock_box_.reset();
  previous_probe_position_.reset();
  tool_setter_overridden_ = false;
}

void ProbeContactModel::set_tool_setter_box(const Box& box) {
  tool_setter_box_ = physical_tooling::normalized(box);
  tool_setter_overridden_ = true;
}

void ProbeContactModel::clear_tool_setter_box() {
  tool_setter_box_.reset();
  tool_setter_overridden_ = true;
}

void ProbeContactModel::configure_tool_setter(std::optional<Box> box) {
  if (!tool_setter_overridden_ && box.has_value()) {
    tool_setter_box_ = *box;
  }
}

void ProbeContactModel::set_stock_box(const Box& box) { stock_box_ = physical_tooling::normalized(box); }

ProbeContactState ProbeContactModel::update(Point3 spindle_probe_position, const AtcSpindleState& spindle_tool) {
  ProbeContactState state;
  const bool spindle_probe_installed =
      spindle_tool.has_tool && (physical_tooling::is_firmware_probe_tool(spindle_tool.tool) ||
                                physical_tooling::is_probe_kind(spindle_tool.kind));
  const bool any_probe_installed = probe_tool_installed_ || spindle_probe_installed;

  state.probe_contact =
      any_probe_installed && stock_box_.has_value() &&
      stock_z_probe_contacts_top_face(*stock_box_, spindle_probe_position, physical_tooling::tool_shank_insert_mm);
  state.tool_setter_contact =
      tool_setter_box_.has_value() && physical_tooling::contains(*tool_setter_box_, spindle_probe_position);
  if (!state.tool_setter_contact && spindle_tool.has_tool && tool_setter_box_.has_value()) {
    state.tool_setter_contact =
        tool_tip_contacts_box(*tool_setter_box_, spindle_probe_position, spindle_tool.length_mm);
  }
  if (!state.probe_contact && any_probe_installed && spindle_tool.has_tool &&
      physical_tooling::is_probe_kind(spindle_tool.kind)) {
    if (stock_box_.has_value()) {
      state.probe_contact = probe_tool_contacts_box(*stock_box_, spindle_probe_position, spindle_tool);
    }
    if (!state.probe_contact && tool_setter_box_.has_value()) {
      state.probe_contact = probe_tool_contacts_box(*tool_setter_box_, spindle_probe_position, spindle_tool);
    }
  }

  if (stock_box_.has_value() && any_probe_installed && previous_probe_position_.has_value()) {
    const bool stock_z_probe_active =
        !spindle_tool.has_tool || (spindle_tool.has_tool && spindle_tool.kind == ToolKind::StockZProbe);
    const double tool_length = spindle_tool.has_tool ? spindle_tool.length_mm : physical_tooling::tool_shank_insert_mm;
    if (stock_z_probe_active) {
      state.stock_probe_crash_axis =
          stock_z_probe_side_crash_axis(*stock_box_, *previous_probe_position_, spindle_probe_position, tool_length);
    }
  }
  previous_probe_position_ = spindle_probe_position;
  return state;
}

}  // namespace sim
