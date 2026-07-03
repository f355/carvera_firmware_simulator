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

#include <string>

#include "ATCHandlerPublicAccess.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/c1_atc_config.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"

namespace {

using sim::test::require;

void pump_script(sim::FirmwareRuntime& runtime) {
  sim::RuntimePumpOptions options;
  options.main_loop_iterations = 8;
  options.max_step_ticks = 100'000;
  runtime.pump(options);
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_atc_m6_runtime");
  sim::test::write_c1_atc_config(sd.path());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& scene = simulation.machine().context().physical_scene();
  scene.set_atc_pocket_tool(1, 1, true, 62.0);
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();
  const auto tool_setter = scene.tool_setter_box();
  require(tool_setter.has_value(), "C1 boot should configure the physical ETS volume");
  require(tool_setter->max_z == -105.5,
          "C1 ETS trigger point should be 1mm below an 8mm button over the configured rack surface");

  runtime.write_serial("M6 T10000000\n");
  require(runtime.run_until_idle(100'000), "invalid tool request should be handled without queued motion");
  auto serial = runtime.read_serial();
  require(kernel.is_halted(), "invalid ATC tool should halt the firmware");
  require(kernel.get_halt_reason() == ATC_TOOL_INVALID, "invalid ATC tool should report ATC_TOOL_INVALID");
  require(serial.find("Invalid tool") != std::string::npos,
          "invalid ATC tool should be reported on the firmware stream");
  require(!scene.atc_spindle().has_tool,
          "invalid ATC tool request should not move a physical tool into the spindle");
  require(scene.atc_pockets().front().occupied,
          "invalid ATC tool request should leave the rack pocket untouched");

  runtime.write_serial("M999\n");
  require(runtime.run_until_idle(100'000), "M999 should recover from invalid-tool ATC halt");
  (void)runtime.read_serial();
  require(!kernel.is_halted(), "invalid-tool recovery should clear the firmware halt");

  runtime.write_serial("M493.2 T1\n");
  require(runtime.run_until_idle(100'000), "direct firmware tool-state command should run");
  (void)runtime.read_serial();
  require(!scene.atc_spindle().has_tool,
          "M493.2 should not physically move a rack tool into the spindle");
  require(scene.atc_pockets().front().occupied,
          "M493.2 should leave the simulated rack pocket occupied");

  runtime.write_serial("M493.2 T-1\n");
  require(runtime.run_until_idle(100'000), "initial tool state command should run");
  (void)runtime.read_serial();

  runtime.write_serial("M6 T1\n");
  serial.clear();
  for (int i = 0; i < 600 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos;
       ++i) {
    pump_script(runtime);
    serial += runtime.read_serial();
  }

  if (serial.find("Start picking new tool: T1") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Start picking new tool: T1") != std::string::npos, "M6 T1 should enter the C1 ATC pick path");
  if (serial.find("Done ATC") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Done ATC") != std::string::npos, "M6 T1 should complete the ATC script");
  require(serial.find("ERROR:") == std::string::npos, "M6 T1 should not hit ATC detector or probe errors");

  tool_status status{};
  require(PublicData::get_value(atc_handler_checksum, get_tool_status_checksum, &status),
          "ATC should publish final tool status");
  require(status.active_tool == 1, "M6 T1 should install tool 1");
  require(status.tool_offset != 0.0F, "TLO measurement should save a non-zero tool offset");

  const auto pockets = scene.atc_pockets();
  require(!pockets.empty() && !pockets.front().occupied, "tool 1 should leave the rack pocket after pickup");
  require(scene.atc_spindle().has_tool, "tool 1 should be held in the simulated spindle");

  scene.set_atc_pocket_tool(2, 2, true, 54.0);
  runtime.write_serial("M6 T2\n");
  serial.clear();
  for (int i = 0; i < 900 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos;
       ++i) {
    pump_script(runtime);
    serial += runtime.read_serial();
  }
  if (serial.find("Done ATC") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Done ATC") != std::string::npos, "M6 T2 should complete after dropping the active T1");
  require(serial.find("ERROR:") == std::string::npos, "M6 T2 should not hit ATC detector or probe errors");

  const auto swapped_pockets = scene.atc_pockets();
  require(swapped_pockets.size() >= 2, "tool swap should leave both configured rack pockets visible");
  require(swapped_pockets[0].occupied && swapped_pockets[0].tool == 1, "M6 T2 should drop old T1 back into pocket 1");
  require(!swapped_pockets[1].occupied, "M6 T2 should remove new T2 from pocket 2");
  require(scene.atc_spindle().has_tool && scene.atc_spindle().tool == 2,
          "M6 T2 should leave the simulated spindle holding T2");

  scene.set_atc_pocket_tool(0, 0, true, 50.0);
  runtime.write_serial("M6 T0\n");
  serial.clear();
  for (int i = 0;
       i < 1200 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos; ++i) {
    pump_script(runtime);
    serial += runtime.read_serial();
  }
  if (serial.find("Done ATC") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Done ATC") != std::string::npos, "M6 T0 should pick and calibrate the C1 wireless probe");
  require(serial.find("ERROR:") == std::string::npos, "M6 T0 should not report a dead/unset wireless probe");
  require(scene.probe_tool_installed(),
          "picking C1 probe pocket T0 should make the spindle probe physically active");
  require(scene.atc_spindle().has_tool && scene.atc_spindle().tool == 0,
          "M6 T0 should leave the simulated spindle holding the wireless probe");

  runtime.write_serial("M6 T1\n");
  serial.clear();
  for (int i = 0;
       i < 1200 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos; ++i) {
    pump_script(runtime);
    serial += runtime.read_serial();
  }
  require(serial.find("Done ATC") != std::string::npos, "M6 T1 should drop the wireless probe and pick T1");
  require(!scene.probe_tool_installed(),
          "dropping C1 probe pocket T0 should make the spindle probe inactive");

  scene.set_atc_pocket_tool(3, 3, false, 48.0);
  runtime.write_serial("M6 T3\n");
  serial.clear();
  for (int i = 0; i < 900 && serial.find("ERROR:") == std::string::npos && serial.find("Done ATC") == std::string::npos;
       ++i) {
    pump_script(runtime);
    serial += runtime.read_serial();
  }
  require(serial.find("ERROR:") != std::string::npos, "M6 T3 should report an ATC error when the rack pocket is empty");
  require(
      !scene.atc_spindle().has_tool || scene.atc_spindle().tool != 3,
      "empty rack pickup should not invent a physical T3 in the spindle");

  return 0;
}
