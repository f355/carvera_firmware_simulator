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

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "ATCHandlerPublicAccess.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "sim/machine_geometry.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/physical_scene.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void require_near(double actual, double expected, double tolerance, const char* message) {
  if (std::abs(actual - expected) > tolerance) {
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    std::exit(1);
  }
}

sim::RuntimePumpOptions button_scan_options() {
  sim::RuntimePumpOptions options;
  options.main_loop_iterations = 64;
  options.max_timer_events = 20'000;
  options.timer_budget_policy = sim::TimerBudgetMode::SpendFullBudget;
  return options;
}

void press_front_button(sim::FirmwareRuntime& runtime) {
  const auto options = button_scan_options();
  runtime.inputs().set_main_button_pressed(true);
  runtime.runner().pump(options);
  runtime.inputs().set_main_button_pressed(false);
  runtime.runner().pump(options);
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_ca1_manual_toolchange");
  sd.write_config_txt("");
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0}),
          "CA1 factory settings should apply before boot");
  auto& scene = simulation.machine().context().physical_scene();
  scene.set_atc_pocket_tool(2, 2, true, 58.0);
  (void)runtime.boot();
  require(runtime.is_homed(), "CA1 manual tool-change test should start from a homed machine");

  runtime.io().write_serial("M6 T2\n");
  std::string serial;
  for (int i = 0; i < 120 && (serial.find("Please change the tool to: T2") == std::string::npos ||
                              !runtime.boot().is_tool_waiting());
       ++i) {
    runtime.runner().pump_free_running(8, 100'000);
    serial += runtime.io().read_serial();
  }
  if (serial.find("Please change the tool to: T2") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Please change the tool to: T2") != std::string::npos,
          "CA1 M6 T2 should enter the manual tool-change path");
  if (!runtime.boot().is_tool_waiting()) {
    std::cerr << "tool waiting was false; serial was:\n" << serial << '\n';
  }
  require(runtime.boot().is_tool_waiting(), "CA1 manual tool change should wait for controller or button confirm");

  scene.set_spindle_tool(2, 58.0, true);
  press_front_button(runtime);
  require(!runtime.boot().is_tool_waiting(), "front button should confirm CA1 manual tool change waiting state");
  for (int i = 0; i < 800 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos;
       ++i) {
    runtime.runner().pump_free_running(8, 100'000);
    serial += runtime.io().read_serial();
  }
  if (serial.find("Done ATC") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Done ATC") != std::string::npos,
          "CA1 confirm should continue through ETS probing and finish the manual tool change");
  require(serial.find("ERROR:") == std::string::npos, "CA1 manual tool change should not hit probe errors");

  tool_status status{};
  require(PublicData::get_value(atc_handler_checksum, get_tool_status_checksum, &status),
          "CA1 ATCHandler should publish manual tool status");
  require(status.active_tool == 2, "CA1 M6 T2 should install tool 2");
  if (status.tool_offset == 0.0F) {
    std::cerr << serial << '\n';
  }
  require(status.tool_offset != 0.0F, "CA1 ETS probing should save a non-zero TLO");
  const auto ca1_geometry = sim::geometry_for(sim::MachineModel::CarveraAirCA1);
  require(ca1_geometry.has_value(), "CA1 tool-change test requires physical ETS geometry");
  const double cutting_stickout = 58.0 - 20.0;
  require_near(status.cur_tool_mz, ca1_geometry->tool_setter.max_z + cutting_stickout, 0.05,
               "CA1 firmware TLO state should preserve its machine-coordinate homing offset");

  const auto spindle = scene.atc_spindle();
  require(spindle.has_tool && spindle.tool == 2, "CA1 virtual tool 2 should be held in the spindle");
  require(spindle.length_mm == 58.0, "CA1 virtual tool length should come from the simulator tool table");
  return 0;
}
