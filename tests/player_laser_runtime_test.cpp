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

#include <iostream>
#include <string>

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "sim/physical_scene.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"

extern "C" void TIMER2_IRQHandler(void);
#include "support/assertions.hpp"

using sim::test::require;

namespace {

std::string laser_config() {
  sim::test::CartesianConfigOptions options;
  options.include_probe_inputs = true;
  options.extra =
      "laser_module_enable true\n"
      "laser_module_pin 2.12\n"
      "laser_module_pwm_pin 2.4\n"
      "laser_module_ttl_pin nc\n"
      "laser_module_pwm_period 1000\n"
      "laser_module_test_power 0.01\n"
      "laser_module_maximum_power 1.0\n"
      "laser_module_minimum_power 0.0\n";
  return sim::test::cartesian_config(options);
}

sim::RuntimePumpOptions button_scan_options() {
  sim::RuntimePumpOptions options;
  options.main_loop_iterations = 64;
  options.max_timer_events = 20'000;
  options.timer_budget_policy = sim::TimerBudgetMode::SpendFullBudget;
  return options;
}

void select_and_start(sim::FirmwareRuntime& runtime, const std::string& name) {
  runtime.io().write_serial_command("M23 " + name + "\n");
  runtime.runner().run_main_loop(1);
  runtime.io().write_serial_command("M24\n");
  runtime.runner().run_main_loop(1);
  (void)runtime.io().read_serial_text();
}

void pump_player_line(sim::FirmwareRuntime& runtime) { runtime.runner().run_main_loop(1); }

void pump_timer_ticks(sim::FirmwareRuntime& runtime, std::size_t ticks) {
  sim::RuntimePumpOptions options;
  options.max_timer_events = ticks;
  runtime.runner().pump(options);
}

void run_slow_ticker() {
  TIMER2_IRQHandler();
  TIMER2_IRQHandler();
}

void press_front_button(sim::FirmwareRuntime& runtime) {
  const auto options = button_scan_options();
  runtime.inputs().set_main_button_pressed(true);
  runtime.runner().pump(options);
  runtime.inputs().set_main_button_pressed(false);
  runtime.runner().pump(options);
}

void test_player_laser_test_mode(sim::FirmwareRuntime& runtime) {
  select_and_start(runtime, "laser_test");

  pump_player_line(runtime);
  auto laser = runtime.inputs().laser_state();
  require(laser.available, "runtime should expose real Laser state");
  require(laser.mode, "Player file should switch firmware Laser into laser mode");
  require(laser.testing, "Player file M323 should enable Laser test mode through firmware");
  run_slow_ticker();
  laser = runtime.inputs().laser_state();
  require(laser.power_percent > 0.0F, "Player file M323 should drive test PWM through firmware");

  pump_player_line(runtime);
  laser = runtime.inputs().laser_state();
  require(laser.mode, "M324 should stop testing without leaving laser mode");
  require(!laser.testing, "Player file M324 should turn Laser test mode off");
  run_slow_ticker();
  laser = runtime.inputs().laser_state();
  require(laser.power_percent == 0.0F, "M324 should settle Laser test PWM off on the next slow tick");

  pump_player_line(runtime);
  laser = runtime.inputs().laser_state();
  require(!laser.mode, "Player file should return firmware Laser to CNC mode");
  require(!laser.firing, "Laser should remain off after returning to CNC mode");
}

void test_player_laser_cutting_move(sim::FirmwareRuntime& runtime, sim::MachineSimulator& simulator) {
  select_and_start(runtime, "laser_cut");

  pump_player_line(runtime);
  auto laser = runtime.inputs().laser_state();
  require(laser.mode, "Player cutting file should enter firmware Laser mode");

  pump_player_line(runtime);
  pump_player_line(runtime);
  laser = runtime.inputs().laser_state();
  require(laser.firing, "M3 in Player cutting file should arm Laser firing");
  require(laser.power_percent == 0.0F, "armed Laser should remain at zero PWM before a cutting block runs");

  const double start_x = simulator.axis_position_mm(0);
  pump_player_line(runtime);
  bool saw_motion_power = false;
  for (int i = 0; i < 20; ++i) {
    pump_timer_ticks(runtime, 100);
    run_slow_ticker();
    laser = runtime.inputs().laser_state();
    if (laser.power_percent > 0.0F) {
      saw_motion_power = true;
      break;
    }
  }

  require(simulator.axis_position_mm(0) < start_x, "Player cutting move should move the physical X axis");
  require(saw_motion_power, "Player cutting move should drive Laser PWM from the active motion block");

  runtime.runner().run_until_motion_idle(200'000);
  pump_player_line(runtime);
  pump_player_line(runtime);
  pump_timer_ticks(runtime, 4);
  laser = runtime.inputs().laser_state();
  require(!laser.firing, "M5 in Player cutting file should stop Laser firing");
  require(!laser.mode, "Player cutting file should return to CNC mode");
}

void test_ca1_plain_laser_mode_requests_manual_laser_tool() {
  sim::test::TempSdCard sd("carvera_sim_ca1_plain_laser_mode_test");
  sd.write_config(laser_config());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0}),
          "CA1 factory settings should apply before boot");
  auto& kernel = runtime.boot();
  require(!kernel.is_halted(), "CA1 runtime should boot before plain M321 laser test");
  (void)runtime.io().read_serial_text();

  runtime.io().write_serial_command("M321\n");
  std::string serial;
  for (int i = 0; i < 120 && !kernel.is_tool_waiting(); ++i) {
    runtime.runner().pump_free_running(8, 100'000);
    serial += runtime.io().read_serial_text();
  }
  if (!kernel.is_tool_waiting()) {
    std::cerr << serial << '\n';
  }
  require(kernel.is_tool_waiting(), "plain M321 on CA1 should request the firmware laser tool through real ATCHandler");
  require(runtime.inputs().laser_state().mode, "plain M321 should enter laser mode while waiting for the laser tool");

  auto& scene = simulation.machine().context().physical_scene();
  scene.set_spindle_tool(8888, 45.0, true);
  press_front_button(runtime);
  require(!kernel.is_tool_waiting(), "front button should confirm the CA1 laser tool change");
  for (int i = 0; i < 800 && serial.find("Done ATC") == std::string::npos && serial.find("ERROR:") == std::string::npos;
       ++i) {
    runtime.runner().pump_free_running(8, 100'000);
    serial += runtime.io().read_serial_text();
  }
  if (serial.find("Done ATC") == std::string::npos) {
    std::cerr << serial << '\n';
  }
  require(serial.find("Done ATC") != std::string::npos,
          "confirmed CA1 laser tool change should finish through the real ATC/TLO script");
  require(serial.find("ERROR:") == std::string::npos, "plain M321 CA1 laser tool change should not hit probe errors");
  require(runtime.inputs().laser_state().mode, "laser mode should remain active after installing the CA1 laser tool");
  const auto spindle = scene.atc_spindle();
  require(spindle.has_tool && spindle.tool == 8888, "virtual CA1 laser tool should remain in the spindle");

  runtime.io().write_serial_command("M322\n");
  require(runtime.runner().run_until_motion_idle(100'000).motion_idle, "M322 should settle after CA1 laser tool test");
  require(!runtime.inputs().laser_state().mode, "M322 should return the CA1 runtime to CNC mode");
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_player_laser_runtime_test");
  sd.write_config(laser_config());
  sd.write("gcodes/laser_test.cnc",
           "M321.2\n"
           "M323\n"
           "M324\n"
           "M322.2\n");
  sd.write("gcodes/laser_cut.cnc",
           "M321.2\n"
           "G91\n"
           "M3 S0.5\n"
           "G1 X-20 F60\n"
           "M5\n"
           "M322.2\n");
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();
  require(!kernel.is_halted(), "runtime should boot before Player laser playback");
  (void)runtime.io().read_serial_text();

  test_player_laser_test_mode(runtime);
  test_player_laser_cutting_move(runtime, simulator);
  test_ca1_plain_laser_mode_requests_manual_laser_tool();
  return 0;
}
