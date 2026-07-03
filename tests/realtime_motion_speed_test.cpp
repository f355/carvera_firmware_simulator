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

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "libs/Kernel.h"
#include "sim/i2c_eeprom.hpp"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"

namespace {

constexpr char kCa1SpeedConfig[] =
    "arm_solution cartesian\n"
    "default_feed_rate 1000\n"
    "default_seek_rate 3000\n"
    "acceleration 150\n"
    "junction_deviation 0.01\n"
    "x_axis_max_speed 4000\n"
    "y_axis_max_speed 4000\n"
    "z_axis_max_speed 3000\n"
    "alpha_step_pin 1.28\n"
    "alpha_dir_pin 1.29\n"
    "alpha_en_pin 0.1\n"
    "alpha_steps_per_mm 640\n"
    "alpha_max_rate 3000\n"
    "alpha_acceleration 150\n"
    "alpha_max_endstop 0.24^\n"
    "alpha_homing_direction home_to_max\n"
    "alpha_max 0\n"
    "alpha_max_travel 500\n"
    "alpha_fast_homing_rate_mm_s 15\n"
    "alpha_slow_homing_rate_mm_s 3\n"
    "alpha_homing_retract_mm 2\n"
    "beta_step_pin 1.26\n"
    "beta_dir_pin 1.27!\n"
    "beta_en_pin 0.0\n"
    "beta_steps_per_mm 640\n"
    "beta_max_rate 3000\n"
    "beta_acceleration 150\n"
    "beta_max_endstop 0.25^\n"
    "beta_homing_direction home_to_max\n"
    "beta_max 0\n"
    "beta_max_travel 380\n"
    "beta_fast_homing_rate_mm_s 15\n"
    "beta_slow_homing_rate_mm_s 3\n"
    "beta_homing_retract_mm 2\n"
    "gamma_step_pin 1.24\n"
    "gamma_dir_pin 1.25\n"
    "gamma_en_pin 3.25\n"
    "gamma_steps_per_mm 640\n"
    "gamma_max_rate 2000\n"
    "gamma_acceleration 150\n"
    "gamma_max_endstop 1.1^\n"
    "gamma_homing_direction home_to_max\n"
    "gamma_max 0\n"
    "gamma_max_travel 150\n"
    "gamma_fast_homing_rate_mm_s 10\n"
    "gamma_slow_homing_rate_mm_s 3\n"
    "gamma_homing_retract_mm 2\n"
    "endstop_debounce_ms 0\n"
    "soft_endstop.enable false\n"
    "sd_ok true\n";

struct MeasuredMove {
  double elapsed_s{};
  double distance_mm{};
  double speed_mm_s{};
};

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

MeasuredMove measure_mid_move(sim::FirmwareRuntime& runtime, sim::MachineSimulator& simulator, double start_x,
                              double end_x) {
  while (simulator.axis_position_mm(0) > start_x) {
    runtime.pump_free_running(4, 1'000);
  }
  const auto started = std::chrono::steady_clock::now();
  const double measured_start_x = simulator.axis_position_mm(0);
  const double measured_start_y = simulator.axis_position_mm(1);
  while (simulator.axis_position_mm(0) > end_x) {
    runtime.pump_free_running(4, 1'000);
  }
  const auto ended = std::chrono::steady_clock::now();
  const double measured_end_x = simulator.axis_position_mm(0);
  const double measured_end_y = simulator.axis_position_mm(1);

  const double elapsed_s = std::chrono::duration<double>(ended - started).count();
  const double distance_mm = std::hypot(measured_end_x - measured_start_x, measured_end_y - measured_start_y);
  return MeasuredMove{elapsed_s, distance_mm, distance_mm / elapsed_s};
}

MeasuredMove run_long_diagonal(double realtime_speed) {
  sim::i2c_eeprom::reset();
  sim::test::TempSdCard sd("carvera_sim_realtime_motion_speed_test");
  sd.write_config_txt(kCa1SpeedConfig);
  sd.mount();

  sim::SimulationInstance simulation;
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 2}),
          "CA1 factory settings should apply before boot");
  auto& kernel = runtime.boot();
  require(runtime.is_homed(), "runtime should boot and home");
  require(!kernel.is_halted(), "runtime should not halt during boot");

  simulator.start_realtime();
  require(simulator.set_realtime_speed(realtime_speed), "test realtime speed should be accepted");
  runtime.write_wifi_tcp("G91\nG1 X-260 Y-180 F3000\n");

  auto result = measure_mid_move(runtime, simulator, -52.0, -152.0);
  runtime.run_until_idle(400'000);
  return result;
}

MeasuredMove run_long_diagonal_after_mid_move_speed_change() {
  sim::i2c_eeprom::reset();
  sim::test::TempSdCard sd("carvera_sim_realtime_motion_speed_change_test");
  sd.write_config_txt(kCa1SpeedConfig);
  sd.mount();

  sim::SimulationInstance simulation;
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 2}),
          "CA1 factory settings should apply before boot");
  auto& kernel = runtime.boot();
  require(runtime.is_homed(), "runtime should boot and home");
  require(!kernel.is_halted(), "runtime should not halt during boot");

  simulator.start_realtime();
  require(simulator.set_realtime_speed(1.0), "1x realtime speed should be accepted");
  runtime.write_wifi_tcp("G91\nG1 X-260 Y-180 F3000\n");
  while (simulator.axis_position_mm(0) > -52.0) {
    runtime.pump_free_running(4, 1'000);
  }
  require(simulator.set_realtime_speed(4.0), "mid-move realtime speed should be accepted");

  auto result = measure_mid_move(runtime, simulator, -62.0, -162.0);
  runtime.run_until_idle(400'000);
  return result;
}

}  // namespace

int main() {
  const auto baseline = run_long_diagonal(1.0);
  std::cerr << "1x speed=" << baseline.speed_mm_s << " mm/s elapsed=" << baseline.elapsed_s << " s\n";
  require(baseline.speed_mm_s > 20.0 && baseline.speed_mm_s < 80.0,
          "1x realtime should execute the firmware's F3000 long-diagonal move at a plausible wall-clock speed");

  const auto accelerated = run_long_diagonal(4.0);
  std::cerr << "4x speed=" << accelerated.speed_mm_s << " mm/s elapsed=" << accelerated.elapsed_s << " s\n";
  require(accelerated.speed_mm_s > baseline.speed_mm_s * 1.5,
          "4x realtime should materially accelerate the same long diagonal");

  const auto mid_move_accelerated = run_long_diagonal_after_mid_move_speed_change();
  std::cerr << "mid-move 4x speed=" << mid_move_accelerated.speed_mm_s
            << " mm/s elapsed=" << mid_move_accelerated.elapsed_s << " s\n";
  require(mid_move_accelerated.speed_mm_s > baseline.speed_mm_s * 1.5,
          "changing realtime speed during a move should accelerate subsequent step timing");
  return 0;
}
