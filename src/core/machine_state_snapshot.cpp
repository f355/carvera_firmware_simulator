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

#include "sim/machine_state_snapshot.hpp"

#include <algorithm>
#include <cstddef>

#include "ATCHandlerPublicAccess.h"
#include "PublicData.h"
#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/machine_geometry.hpp"
#include "sim/runtime_atc_config.hpp"
#include "sim/stepper_axis.hpp"

namespace sim {
namespace {

constexpr std::size_t max_stock_axis_count = 5;

bool firmware_axes_homed(Kernel& kernel) {
  if (kernel.robot == nullptr) {
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (!kernel.robot->is_homed(static_cast<uint8_t>(axis))) {
      return false;
    }
  }
  return true;
}

std::size_t resolved_axis_count(Kernel& kernel, const MachineStateSnapshotOptions& options) {
  if (options.axis_count.has_value()) {
    return std::min(max_stock_axis_count, *options.axis_count);
  }
  if (kernel.robot == nullptr) {
    return std::min(max_stock_axis_count, stepper_axes::count());
  }
  return std::min<std::size_t>(kernel.robot->get_number_registered_motors(), max_stock_axis_count);
}

std::optional<Box> firmware_work_area(Kernel& kernel) {
  if (kernel.robot == nullptr) {
    return std::nullopt;
  }
  return Box{
      kernel.robot->get_soft_endstop_min(0), kernel.robot->get_soft_endstop_min(1),
      kernel.robot->get_soft_endstop_min(2), kernel.robot->get_soft_endstop_max(0),
      kernel.robot->get_soft_endstop_max(1), kernel.robot->get_soft_endstop_max(2),
  };
}

std::optional<Box> physical_travel(MachineModel model, const std::optional<Box>& work_area) {
  if (const auto geometry = geometry_for(model)) {
    return geometry->physical_travel;
  }
  return work_area;
}

AtcSpindleMachineState atc_spindle_state() {
  AtcSpindleMachineState spindle;
  tool_status tool_status_data{};
  if (PublicData::get_value(atc_handler_checksum, get_tool_status_checksum, &tool_status_data)) {
    spindle.active_tool = tool_status_data.active_tool;
    spindle.target_tool = tool_status_data.target_tool;
    spindle.tool_offset_mm = tool_status_data.tool_offset;
    spindle.cur_tool_mz = tool_status_data.cur_tool_mz;
    spindle.ref_tool_mz = tool_status_data.ref_tool_mz;
    spindle.target_collet_type = tool_status_data.target_collet_type;
  }

  const auto physical_spindle = physical_scene::active().atc_spindle();
  spindle.has_tool = physical_spindle.has_tool;
  spindle.tool = physical_spindle.tool;
  spindle.length_mm = physical_spindle.length_mm;
  spindle.kind = physical_spindle.kind;
  spindle.probe_tip_diameter_mm = physical_spindle.probe_tip_diameter_mm;
  if (spindle.has_tool) {
    spindle.active_tool = spindle.tool;
  }
  return spindle;
}

}  // namespace

MachineStateSnapshot assemble_machine_state(Kernel& kernel, bool homed, MachineModel model,
                                            const MachineStateSnapshotOptions& options) {
  MachineStateSnapshot state;
  state.firmware_booted = true;
  state.homed = homed;
  state.soft_endstop_enabled = kernel.robot != nullptr && kernel.robot->is_soft_endstop_enabled();
  state.work_area = firmware_work_area(kernel);
  state.physical_travel = physical_travel(model, state.work_area);
  state.spindle = spindle_state::snapshot(model);

  const auto axis_count = resolved_axis_count(kernel, options);
  state.axes.reserve(axis_count);
  for (std::size_t axis = 0; axis < axis_count; ++axis) {
    AxisMachineState axis_state;
    axis_state.axis = axis;
    if (kernel.robot != nullptr) {
      axis_state.machine_position = kernel.robot->get_axis_position(static_cast<int>(axis));
    }
    if (axis < stepper_axes::count()) {
      axis_state.physical_steps = stepper_axes::position_steps(axis);
      axis_state.physical_mm = stepper_axes::position_mm(axis);
      axis_state.endstop_triggered = stepper_axes::endstop_triggered(axis);
    }
    state.axes.push_back(axis_state);
  }

  if (options.refresh_physical_scene) {
    runtime_atc::configure_physical_scene(kernel);
  }
  state.tool_setter = physical_scene::active().tool_setter_box();
  state.atc.available = physical_scene::active().atc_available();
  state.atc.spindle = atc_spindle_state();
  state.atc.pockets = physical_scene::active().atc_pockets();
  return state;
}

MachineStateSnapshot assemble_machine_state(Kernel& kernel, MachineModel model,
                                            const MachineStateSnapshotOptions& options) {
  return assemble_machine_state(kernel, firmware_axes_homed(kernel), model, options);
}

}  // namespace sim
