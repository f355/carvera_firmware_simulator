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
#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "sim/motion_telemetry.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"
#include "support/assertions.hpp"

using sim::test::require;
using sim::test::require_near;


int main() {
  sim::test::TempSdCard sd("carvera_sim_free_running_homing_test");
  sd.write_config_txt("sd_ok true\nsoft_endstop.enable true\n");
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  sim::MachineTelemetry last_telemetry;
  bool saw_telemetry = false;
  simulation.machine().context().motion_telemetry().set_sink([&](const sim::MachineTelemetry& sample) {
    last_telemetry = sample;
    saw_telemetry = true;
  });
  auto& kernel = runtime.boot();

  require(runtime.is_homed(), "runtime should home the machine during boot");
  simulation.machine().context().motion_telemetry().observe(simulation.machine().context(), kernel, true);
  require(saw_telemetry, "forced motion telemetry should emit after boot homing");
  require(last_telemetry.homed, "motion telemetry should report the real firmware homed state");
  require_near(kernel.robot->get_axis_position(0), -2.0, 0.0001,
               "C1 boot homing should finish at the firmware homing backoff X position");
  require_near(kernel.robot->get_axis_position(1), -2.0, 0.0001,
               "C1 boot homing should finish at the firmware homing backoff Y position");
  require_near(kernel.robot->get_axis_position(2), -2.0, 0.0001,
               "C1 boot homing should finish at the firmware homing backoff Z position");
  require_near(simulator.axis_position_mm(0), -1.0, 0.05,
               "physical X should reflect the max switch sitting outside firmware soft travel");
  require_near(simulator.axis_position_mm(1), -1.0, 0.05,
               "physical Y should reflect the max switch sitting outside firmware soft travel");
  require_near(simulator.axis_position_mm(2), -1.0, 0.05,
               "physical Z should reflect the max switch sitting outside firmware soft travel");
  require(!simulator.axis_endstop_triggered(0), "X homing endstop should release after boot homing backoff");
  require(!simulator.axis_endstop_triggered(1), "Y homing endstop should release after boot homing backoff");
  require(!simulator.axis_endstop_triggered(2), "Z homing endstop should release after boot homing backoff");

  runtime.io().write_serial("?");
  runtime.runner().pump_free_running();
  const auto status = runtime.io().read_serial();
  require(status.find("<Idle") != std::string::npos,
          "runtime should answer controller status polling in free-running mode");

  runtime.io().write_serial("$J X10 Y10 Z10 F10000\n");
  runtime.runner().run_until_motion_idle(200'000);
  const auto jog_response = runtime.io().read_serial();
  require(
      jog_response.find("Hard limit") == std::string::npos && jog_response.find("Limit switch") == std::string::npos,
      "controller-style positive jog past the soft boundary should not trip a hard limit");
  require(kernel.get_halt_reason() != HARD_LIMIT, "soft-limit jogging should not leave firmware in hard-limit alarm");

  runtime.reset();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0}),
          "CA1 factory settings should apply before reboot");
  auto& ca1_kernel = runtime.boot();
  require(runtime.is_homed(), "CA1 runtime should home the machine during boot");
  require(ca1_kernel.robot->is_soft_endstop_enabled(), "CA1 simulator should guard controller jogs with soft limits");
  saw_telemetry = false;
  simulation.machine().context().motion_telemetry().observe(simulation.machine().context(), ca1_kernel, true);
  require(saw_telemetry, "forced CA1 motion telemetry should emit after boot homing");
  require(last_telemetry.physical_travel.has_value(),
          "CA1 motion telemetry should carry physical geometry before GUI movement starts");
  require_near(last_telemetry.physical_travel->min_x, -303.0, 0.0001, "CA1 telemetry should expose hardware X travel");
  require_near(last_telemetry.physical_travel->max_z, 1.0, 0.0001, "CA1 telemetry should expose hardware Z travel");

  runtime.io().write_serial("$J Y10 F10000\n");
  runtime.runner().run_until_motion_idle(200'000);
  const auto ca1_jog_response = runtime.io().read_serial();
  require(ca1_jog_response.find("Hard limit") == std::string::npos &&
              ca1_jog_response.find("Limit switch") == std::string::npos,
          "CA1 positive Y jog should stop at the soft boundary instead of tripping the shared hard switch");
  require(ca1_kernel.get_halt_reason() != HARD_LIMIT, "CA1 soft-limited jog should not leave firmware halted");
  require(!simulator.axis_endstop_triggered(1, sim::EndstopSide::Max),
          "CA1 positive Y jog should remain clear of the physical max switch");

  runtime.io().write_wifi_tcp("G91\nG0 X-100 F1500\n");
  bool saw_motion = false;
  for (int i = 0; i < 200 && !saw_motion; ++i) {
    saw_motion = !runtime.runner().pump({4, 4'000}).motion_idle;
  }
  require(saw_motion, "CA1 e-stop regression should interrupt active air-cut motion");

  runtime.inputs().set_e_stop_pressed(true);
  for (int i = 0; i < 40 && !ca1_kernel.is_halted(); ++i) {
    runtime.runner().pump_free_running(4, 4'000);
  }
  require(ca1_kernel.is_halted(), "CA1 e-stop should halt active motion");
  require(ca1_kernel.get_halt_reason() == E_STOP, "CA1 active-motion e-stop should report E_STOP");

  runtime.inputs().set_e_stop_pressed(false);
  runtime.io().write_wifi_tcp("$X\nG53 G0 Z-2\n");
  runtime.runner().run_until_motion_idle(200'000);
  const auto unlock_response = runtime.io().read_wifi_tcp();
  require(unlock_response.find("motor alarm triggered") == std::string::npos,
          "CA1 e-stop unlock should not turn shared enable/alarm pins into motor alarms");
  require(ca1_kernel.get_halt_reason() != MOTOR_ERROR_X,
          "CA1 e-stop unlock should not re-halt on the X motor alarm input");
  return 0;
}
