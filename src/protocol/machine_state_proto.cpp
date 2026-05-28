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

#include "sim/machine_state_proto.hpp"

#include <cstdint>

#include "sim/api_conversions.hpp"
#include "sim/proto_axis.hpp"

namespace sim::api {
namespace {

void fill_atc_spindle_proto(carvera::sim::v1::ToolState& target, const AtcSpindleMachineState& source) {
  target.set_active_tool(source.active_tool);
  target.set_target_tool(source.target_tool);
  target.set_tool_offset_mm(source.tool_offset_mm);
  target.set_cur_tool_mz(source.cur_tool_mz);
  target.set_ref_tool_mz(source.ref_tool_mz);
  target.set_target_collet_type(source.target_collet_type);
  if (source.has_tool) {
    target.set_active_tool(source.tool);
    target.set_length_mm(source.length_mm);
    target.set_kind(proto_tool_kind(source.kind));
    target.set_probe_tip_diameter_mm(source.probe_tip_diameter_mm);
  }
}

}  // namespace

void fill_box_proto(carvera::sim::v1::Box& target, const Box& source) {
  target.set_min_x(source.min_x);
  target.set_min_y(source.min_y);
  target.set_min_z(source.min_z);
  target.set_max_x(source.max_x);
  target.set_max_y(source.max_y);
  target.set_max_z(source.max_z);
}

void fill_axis_state_proto(carvera::sim::v1::AxisState& target, const AxisMachineState& source) {
  target.set_axis(proto_axis(source.axis));
  target.set_physical_steps(source.physical_steps);
  target.set_physical_mm(source.physical_mm);
  target.set_machine_position(source.machine_position);
  target.set_endstop_triggered(source.endstop_triggered);
}

void fill_spindle_state_proto(carvera::sim::v1::SpindleState& target, const spindle_state::Snapshot& source) {
  target.set_spinning(source.spinning);
  target.set_actual_rpm(source.actual_rpm);
  target.set_target_rpm(source.target_rpm);
  target.set_max_rpm(source.max_rpm);
}

void fill_atc_state_proto(carvera::sim::v1::AtcState& target, const MachineStateSnapshot& source) {
  target.set_available(source.atc.available);
  fill_atc_spindle_proto(*target.mutable_spindle(), source.atc.spindle);
  for (const auto& pocket : source.atc.pockets) {
    auto* output = target.add_pockets();
    output->set_pocket(static_cast<std::uint32_t>(pocket.pocket));
    output->set_tool(pocket.tool);
    output->set_occupied(pocket.occupied);
    output->set_length_mm(pocket.length_mm);
    output->set_x(pocket.position.x);
    output->set_y(pocket.position.y);
    output->set_z(pocket.position.z);
    output->set_kind(proto_tool_kind(pocket.kind));
    output->set_probe_tip_diameter_mm(pocket.probe_tip_diameter_mm);
  }
}

void fill_machine_snapshot_proto(carvera::sim::v1::MachineSnapshot& target, const MachineStateSnapshot& source) {
  target.set_firmware_booted(source.firmware_booted);
  target.set_homed(source.homed);
  target.set_soft_endstop_enabled(source.soft_endstop_enabled);
  if (source.work_area.has_value()) {
    fill_box_proto(*target.mutable_work_area(), *source.work_area);
  }
  if (source.physical_travel.has_value()) {
    fill_box_proto(*target.mutable_physical_travel(), *source.physical_travel);
  }
  fill_spindle_state_proto(*target.mutable_spindle(), source.spindle);
  for (const auto& axis : source.axes) {
    fill_axis_state_proto(*target.add_axes(), axis);
  }
  if (source.tool_setter.has_value()) {
    target.set_tool_setter_available(true);
    fill_box_proto(*target.mutable_tool_setter(), *source.tool_setter);
  }
  fill_atc_state_proto(*target.mutable_atc(), source);
}

void fill_machine_telemetry_proto(carvera::sim::v1::MachineTelemetry& target, const MachineStateSnapshot& source) {
  target.set_firmware_booted(source.firmware_booted);
  target.set_homed(source.homed);
  if (source.physical_travel.has_value()) {
    fill_box_proto(*target.mutable_physical_travel(), *source.physical_travel);
  }
  fill_spindle_state_proto(*target.mutable_spindle(), source.spindle);
  fill_atc_state_proto(*target.mutable_atc(), source);
  for (const auto& axis : source.axes) {
    fill_axis_state_proto(*target.add_axes(), axis);
  }
}

}  // namespace sim::api
