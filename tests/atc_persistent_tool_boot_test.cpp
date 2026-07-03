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

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/c1_atc_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::require;

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_atc_persistent_tool_boot");
  sim::test::C1AtcConfigOptions config_options;
  config_options.include_soft_limit_coordinates = false;
  config_options.include_cartesian_homing_retracts = false;
  config_options.include_detector_motion_limits = false;
  sim::test::write_c1_atc_config(sd.path(), config_options);
  sd.mount();

  sim::SimulationInstance simulation;
  sim::physical_scene::active().set_atc_pocket_tool(2, 2, true, 57.0);
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();

  runtime.write_serial("M493.2 T2\n");
  require(runtime.run_until_idle(100'000), "direct firmware tool-state command should run");
  (void)runtime.read_serial();
  require(kernel.eeprom_data->TOOL == 2, "M493.2 should persist the active tool in firmware EEPROM data");
  require(!sim::physical_scene::active().atc_spindle().has_tool,
          "M493.2 should not physically move a rack tool into the spindle");

  runtime.reset();
  auto& rebooted_kernel = runtime.boot();
  require(rebooted_kernel.eeprom_data->TOOL == 2, "firmware reboot should reload persisted active tool from EEPROM");

  const auto spindle = sim::physical_scene::active().atc_spindle();
  require(spindle.has_tool && spindle.tool == 2, "boot should reconcile persisted active tool into the spindle");
  require(spindle.length_mm == 57.0, "reconciled spindle tool should keep the configured physical length");

  const auto pockets = sim::physical_scene::active().atc_pockets();
  require(pockets.size() == 1 && pockets.front().tool == 2 && !pockets.front().occupied,
          "boot should remove the persisted active tool from its physical rack pocket");
  return 0;
}
