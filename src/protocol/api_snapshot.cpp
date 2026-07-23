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

#include "sim/api_snapshot.hpp"

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/firmware_runtime.hpp"
#include "sim/lpc_memory_proto.hpp"
#include "sim/machine_state_proto.hpp"
#include "sim/machine_state_snapshot.hpp"
#include "sim/simulator_context.hpp"

namespace sim {
namespace {

bool fill_snapshot_from_kernel(carvera::sim::v1::MachineSnapshot& snapshot, FirmwareRuntime& firmware,
                               MachineSimulator& simulator) {
  auto& kernel = firmware.start();
  if (kernel.robot == nullptr) {
    return false;
  }

  MachineStateSnapshotOptions options;
  options.refresh_physical_scene = true;
  api::fill_machine_snapshot_proto(
      snapshot,
      assemble_machine_state(simulator.context(), kernel, firmware.factory_settings().machine_model, options));
  api::fill_memory_summary(*snapshot.mutable_memory(), simulator.context().memory_accounting().snapshot());
  return true;
}

}  // namespace

bool fill_machine_snapshot(carvera::sim::v1::MachineSnapshot& snapshot, FirmwareRuntime& firmware,
                           MachineSimulator& simulator) {
  (void)firmware.boot();
  return fill_snapshot_from_kernel(snapshot, firmware, simulator);
}

bool fill_machine_snapshot_nonblocking(carvera::sim::v1::MachineSnapshot& snapshot, FirmwareRuntime& firmware,
                                       MachineSimulator& simulator) {
  if (!firmware.booted()) {
    return false;
  }
  return fill_snapshot_from_kernel(snapshot, firmware, simulator);
}

}  // namespace sim
