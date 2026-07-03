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
#include <string>

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/m8266_wifi.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/stepper_axis.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"
#include "support/assertions.hpp"

using sim::test::require;
using sim::test::require_near;

namespace {

std::string ca1_rotary_config() {
  sim::test::CartesianConfigOptions options;
  options.include_rotary_axes = true;
  options.extra =
      "delta_step_pin 1.21\n"
      "delta_dir_pin 1.23!\n"
      "delta_en_pin 1.30\n"
      "delta_steps_per_mm 888.888889\n"
      "delta_max_rate 1800\n"
      "delta_acceleration 360\n"
      "delta_min_endstop 1.9^\n"
      "delta_max_endstop 1.9^\n"
      "delta_homing_direction home_to_min\n"
      "delta_min -0.5\n"
      "delta_max 0\n"
      "delta_max_travel 40\n"
      "delta_fast_homing_rate_mm_s 50\n"
      "delta_slow_homing_rate_mm_s 10\n"
      "delta_limit_enable true\n"
      "delta_homing_retract_mm 0.5\n"
      "watchdog_timeout 0\n";
  return sim::test::cartesian_config(options);
}

void pulse_delta_steps(sim::MachineSimulator& simulator, int pulses) {
  simulator.set_gpio_input({1, 23}, true);
  sim::stepper_axes::on_gpio_level_changed({1, 23}, true);
  for (int i = 0; i < pulses; ++i) {
    simulator.set_gpio_input({1, 21}, true);
    sim::stepper_axes::on_gpio_level_changed({1, 21}, true);
    simulator.set_gpio_input({1, 21}, false);
    sim::stepper_axes::on_gpio_level_changed({1, 21}, false);
  }
}

}  // namespace

void test_ca1_without_rotary_accessory() {
  sim::test::TempSdCard sd("carvera_sim_rotary_axis_disabled_test");
  sd.write_config(ca1_rotary_config());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0x01}),
          "CA1 factory settings may enable A homing even when the accessory is unplugged");
  auto& kernel = runtime.boot();

  require(kernel.robot != nullptr, "runtime boot should create the firmware Robot");
  require(kernel.robot->get_number_registered_motors() >= 4, "rotary config should still register firmware A motor");
  require(!kernel.is_halted(), "firmware boot should not require A motion when the accessory is unplugged");
  require(sim::stepper_axes::count() >= 4,
          "simulator should preserve the firmware A axis slot even when the accessory is disabled");
  require(simulator.axis_endstop_triggered(3),
          "unplugged CA1 rotary accessory should present the A home input as already triggered");
  const double initial_angle = simulator.axis_position_mm(3);
  runtime.io().write_serial("G91 G0 A90 F600\n");
  runtime.runner().run_until_motion_idle(100'000);
  require_near(simulator.axis_position_mm(3), initial_angle, 0.001,
               "unplugged CA1 rotary accessory should ignore A step pulses physically");

  simulator.set_rotary_accessory_installed(true);
  require(!simulator.axis_endstop_triggered(3), "plugged CA1 rotary accessory should expose the live A home switch");
  pulse_delta_steps(simulator, 1000);
  require(simulator.axis_position_mm(3) > initial_angle + 0.05,
          "plugging the CA1 rotary accessory after boot should connect the physical A motor");
}

void test_c1_without_rotary_accessory() {
  sim::test::TempSdCard sd("carvera_sim_c1_rotary_axis_disabled_test");
  sd.write_config(ca1_rotary_config());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraC1, 0x05}),
          "C1 factory settings may enable A homing even when the accessory is unplugged");
  auto& kernel = runtime.boot();

  require(kernel.robot != nullptr, "runtime boot should create the firmware Robot");
  require(kernel.robot->get_number_registered_motors() >= 4, "rotary-capable config should register A/delta motor");
  require(!kernel.is_halted(), "firmware boot should not require A motion when the accessory is unplugged");
  require(sim::stepper_axes::count() >= 4,
          "simulator should preserve the firmware A axis slot even when the accessory is disabled");
  require(simulator.axis_endstop_triggered(3),
          "unplugged C1 rotary accessory should present the A home input as already triggered");
  const double initial_angle = simulator.axis_position_mm(3);
  runtime.io().write_serial("G91 G0 A90 F600\n");
  runtime.runner().run_until_motion_idle(100'000);
  require_near(simulator.axis_position_mm(3), initial_angle, 0.001,
               "unplugged C1 rotary accessory should ignore A step pulses physically");

  simulator.set_rotary_accessory_installed(true);
  require(!simulator.axis_endstop_triggered(3), "plugged C1 rotary accessory should expose the live A home switch");
  pulse_delta_steps(simulator, 1000);
  require(simulator.axis_position_mm(3) > initial_angle + 0.05,
          "plugging the C1 rotary accessory after boot should connect the physical A motor");
}

void test_ca1_with_rotary_accessory() {
  sim::test::TempSdCard sd("carvera_sim_rotary_axis_enabled_test");
  sd.write_config(ca1_rotary_config());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  simulator.set_rotary_accessory_installed(true);
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0x01}),
          "CA1 factory settings should enable the 4th-axis homing flag before boot");
  auto& kernel = runtime.boot();

  require(kernel.robot != nullptr, "runtime boot should create the firmware Robot");
  require(kernel.robot->get_number_registered_motors() >= 4, "CA1 rotary config should register A/delta motor");
  require(sim::stepper_axes::count() >= 4, "simulator should attach a physical A stepper axis");
  require(!kernel.is_halted(), "firmware boot homing should not halt when the A index sensor is reached");
  require_near(kernel.robot->get_axis_position(3), 0.5, 0.001,
               "firmware A machine position should reflect the post-index backoff");
  const double home_angle = simulator.axis_position_mm(3);
  require_near(home_angle, 1.0, 0.05, "physical A chuck should back off from the index sensor");
  require(!simulator.axis_endstop_triggered(3), "A-axis shared homing/limit switch should release after backoff");

  simulation.machine().context().m8266_wifi().connect_tcp_client();
  (void)runtime.io().read_wifi_tcp();
  runtime.io().write_wifi_tcp("G91 G0 A90 F600\n");
  for (int i = 0; i < 2'000 && sim::stepper_axes::count() >= 4 &&
                  std::fabs(simulator.axis_position_mm(3) - (home_angle + 90.0)) > 0.2;
       ++i) {
    runtime.runner().pump_free_running();
  }
  require(sim::stepper_axes::count() >= 4, "simulator should keep the physical A stepper axis after jogging");
  require_near(simulator.axis_position_mm(3), home_angle + 90.0, 0.2,
               "controller-style G-code A jog should rotate the physical chuck");
}

void test_c1_with_rotary_accessory() {
  sim::test::TempSdCard sd("carvera_sim_c1_rotary_axis_enabled_test");
  sd.write_config(ca1_rotary_config());
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& simulator = simulation.machine();
  simulator.set_rotary_accessory_installed(true);
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraC1, 0x05}),
          "C1 factory settings should preserve the C1 flag while enabling A homing");
  auto& kernel = runtime.boot();

  require(kernel.robot != nullptr, "runtime boot should create the firmware Robot");
  require(kernel.robot->get_number_registered_motors() >= 4, "C1 rotary config should register A/delta motor");
  require(!kernel.is_halted(), "C1 firmware boot homing should not halt when the A index sensor is reached");
  require_near(simulator.axis_position_mm(3), 1.0, 0.05, "physical C1 A chuck should back off from the index sensor");
}

int main() {
  test_ca1_without_rotary_accessory();
  test_c1_without_rotary_accessory();
  test_ca1_with_rotary_accessory();
  test_c1_with_rotary_accessory();
  return 0;
}
