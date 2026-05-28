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

#include "Robot.h"
#include "carvera_sim.pb.h"
#include "libs/Kernel.h"
#include "sim/api_snapshot.hpp"
#include "sim/firmware_runtime.hpp"
#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"

int main() {
  using sim::test::require;

  sim::test::TempSdCard sd("carvera_sim_nonblocking_snapshot_homed_test");
  sd.write_config_txt("sd_ok true\nsoft_endstop.enable true\n");
  sd.mount();

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime runtime(simulator);
  auto& kernel = runtime.start();
  require(kernel.robot != nullptr, "firmware start should create Robot");
  require(runtime.run_until_idle(200'000), "firmware should finish startup homing");
  require(kernel.robot->is_homed_all_axes(), "Robot should know that startup homing completed");

  carvera::sim::v1::MachineSnapshot snapshot;
  require(sim::fill_machine_snapshot_nonblocking(snapshot, runtime, simulator),
          "nonblocking snapshot should be available after firmware start");
  require(snapshot.homed(), "nonblocking snapshot should report the live Robot homed state");

  return 0;
}
