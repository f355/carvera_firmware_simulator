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

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"
#include "support/cartesian_config.hpp"
#include "support/runtime_wait.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

bool pump_until_reset(sim::FirmwareRuntime& runtime) {
  sim::RuntimePumpOptions options;
  options.main_loop_iterations = 1;

  for (int i = 0; i < 3; ++i) {
    runtime.boot().call_event(ON_SECOND_TICK);
    if (runtime.runner().pump(options).reset_requested) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_hard_limit_recovery_test");
  const auto& root = temp_root.path();

  sim::test::CartesianConfigOptions config;
  config.extra =
      "alpha_limit_enable true\n"
      "beta_limit_enable true\n"
      "gamma_limit_enable true\n"
      "endstop_debounce_count 1\n";
  sim::test::write_cartesian_config(root, config);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();
  require(!kernel.is_halted(), "boot should finish without an alarm");

  runtime.io().write_serial("G91\n");
  runtime.io().write_serial("G0 X100 F120\n");
  require(sim::test::pump_until_axis_moves_by(runtime.runner(), simulator, kernel, 0, 0.02),
          "test move should start before the hard-limit switch is asserted");
  require(!kernel.is_halted(), "test move should start without an alarm");

  runtime.inputs().set_limit_switch(0, sim::LimitSwitchSide::Max, true);
  require(sim::test::pump_until_halted(runtime.runner(), kernel),
          "real Endstops should halt firmware when a moving axis hits a hard limit");
  const auto hard_limit_output = runtime.io().read_serial();
  require(kernel.get_halt_reason() == HARD_LIMIT, "hard limit should set HARD_LIMIT halt reason");
  require(hard_limit_output.find("Limit switch") != std::string::npos ||
              hard_limit_output.find("Hard limit") != std::string::npos,
          "hard-limit halt should be reported on the firmware stream");

  runtime.io().write_serial("reset\n");
  runtime.runner().run_main_loop(1);
  const auto reset_output = runtime.io().read_serial();
  require(reset_output.find("Rebooting machine") != std::string::npos,
          "controller reset command should go through real SimpleShell reset path");
  require(pump_until_reset(runtime), "simulator runtime should honor firmware system_reset request");

  auto& rebooted_kernel = runtime.boot();
  require(!rebooted_kernel.is_halted(), "runtime should reboot cleanly after controller reset from a hard limit");
  require(runtime.is_homed(), "runtime should home again after controller reset");

  runtime.io().write_serial("G91\n");
  runtime.io().write_serial("G0 X100 F120\n");
  require(sim::test::pump_until_axis_moves_by(runtime.runner(), simulator, rebooted_kernel, 0, 0.02),
          "second test move should start before the hard-limit switch is asserted");
  runtime.inputs().set_limit_switch(0, sim::LimitSwitchSide::Max, true);
  require(sim::test::pump_until_halted(runtime.runner(), rebooted_kernel),
          "second hard limit should halt before M999 recovery");

  runtime.io().write_serial("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  const auto m999_output = runtime.io().read_serial();
  require(!rebooted_kernel.is_halted(),
          "firmware M999 should clear the hard-limit halt even before the switch mechanically releases");
  require(m999_output.find("After HALT you should HOME") != std::string::npos,
          "M999 recovery should warn that the machine must home before motion");

  runtime.inputs().set_limit_switch(0, sim::LimitSwitchSide::Max, false);
  runtime.runner().pump_free_running(8, 5'000);
  require(!rebooted_kernel.is_halted(), "releasing the hard-limit switch should not reassert the alarm after M999");

  runtime.io().write_serial("G0 X-1 F120\n");
  require(sim::test::pump_until_axis_moves_by(runtime.runner(), simulator, rebooted_kernel, 0, 0.02),
          "firmware should accept motion after M999 recovery");
  require(runtime.runner().run_until_motion_idle(200'000).motion_idle, "post-M999 recovery motion should reach idle");
  require(!rebooted_kernel.is_halted(), "post-M999 recovery motion should not reassert the hard-limit alarm");
  return 0;
}
