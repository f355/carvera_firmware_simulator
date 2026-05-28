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

#ifndef SIMULATOR_SIM_MACHINE_STATE_SNAPSHOT_HPP
#define SIMULATOR_SIM_MACHINE_STATE_SNAPSHOT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sim/i2c_eeprom.hpp"
#include "sim/physical_scene.hpp"
#include "sim/spindle_state.hpp"

class Kernel;

namespace sim {

struct AxisMachineState {
  std::size_t axis{0};
  std::int64_t physical_steps{0};
  double physical_mm{0.0};
  double machine_position{0.0};
  bool endstop_triggered{false};
};

struct AtcSpindleMachineState : AtcSpindleState {
  int active_tool{-1};
  int target_tool{-1};
  double tool_offset_mm{0.0};
  double cur_tool_mz{0.0};
  double ref_tool_mz{0.0};
  int target_collet_type{0};
};

struct AtcMachineState {
  bool available{false};
  AtcSpindleMachineState spindle{};
  std::vector<AtcPocketState> pockets{};
};

struct MachineStateSnapshotOptions {
  bool refresh_physical_scene{false};
  std::optional<std::size_t> axis_count{};
};

struct MachineStateSnapshot {
  bool firmware_booted{false};
  bool homed{false};
  bool soft_endstop_enabled{false};
  std::optional<Box> work_area{};
  std::optional<Box> physical_travel{};
  std::optional<Box> tool_setter{};
  spindle_state::Snapshot spindle{};
  AtcMachineState atc{};
  std::vector<AxisMachineState> axes{};
};

MachineStateSnapshot assemble_machine_state(Kernel& kernel, bool homed, MachineModel model,
                                            const MachineStateSnapshotOptions& options = {});
MachineStateSnapshot assemble_machine_state(Kernel& kernel, MachineModel model,
                                            const MachineStateSnapshotOptions& options = {});

}  // namespace sim

#endif
