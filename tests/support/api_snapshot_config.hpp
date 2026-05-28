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

#ifndef SIMULATOR_TESTS_SUPPORT_API_SNAPSHOT_CONFIG_HPP
#define SIMULATOR_TESTS_SUPPORT_API_SNAPSHOT_CONFIG_HPP

#include <filesystem>
#include <fstream>

namespace sim::test {

inline void write_api_snapshot_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "arm_solution cartesian\n"
         << "alpha_step_pin 1.28\n"
         << "alpha_dir_pin 1.29\n"
         << "alpha_en_pin nc\n"
         << "alpha_steps_per_mm 200\n"
         << "alpha_max_rate 3000\n"
         << "alpha_acceleration 150\n"
         << "alpha_min_endstop 0.24^\n"
         << "alpha_max_endstop 0.25^\n"
         << "alpha_homing_direction home_to_max\n"
         << "alpha_min 0\n"
         << "alpha_max 0\n"
         << "alpha_max_travel 20\n"
         << "alpha_limit_enable true\n"
         << "alpha_homing_retract_mm 5\n"
         << "alpha_motor_alarm_pin 0.1!^\n"
         << "beta_step_pin 1.26\n"
         << "beta_dir_pin 1.27\n"
         << "beta_en_pin nc\n"
         << "beta_steps_per_mm 200\n"
         << "beta_max_rate 3000\n"
         << "beta_acceleration 150\n"
         << "beta_min_endstop 1.1^\n"
         << "beta_max_endstop 1.4^\n"
         << "beta_homing_direction home_to_max\n"
         << "beta_min 0\n"
         << "beta_max 0\n"
         << "beta_max_travel 20\n"
         << "beta_limit_enable true\n"
         << "beta_homing_retract_mm 5\n"
         << "beta_motor_alarm_pin 0.0!^\n"
         << "gamma_step_pin 1.24\n"
         << "gamma_dir_pin 1.25\n"
         << "gamma_en_pin nc\n"
         << "gamma_steps_per_mm 200\n"
         << "gamma_max_rate 3000\n"
         << "gamma_acceleration 150\n"
         << "gamma_min_endstop 0.26^\n"
         << "gamma_max_endstop 1.8^\n"
         << "gamma_homing_direction home_to_max\n"
         << "gamma_min 0\n"
         << "gamma_max 0\n"
         << "gamma_max_travel 20\n"
         << "gamma_limit_enable true\n"
         << "gamma_homing_retract_mm 5\n"
         << "gamma_motor_alarm_pin 3.25!^\n"
         << "delta_step_pin 1.18\n"
         << "delta_dir_pin 1.20!\n"
         << "delta_en_pin nc\n"
         << "delta_steps_per_mm 26.666667\n"
         << "delta_max_rate 1800\n"
         << "delta_acceleration 360\n"
         << "delta_min_endstop 0.21!^\n"
         << "delta_max_endstop 0.21!^\n"
         << "delta_homing_direction home_to_min\n"
         << "delta_min -0.5\n"
         << "delta_max 0\n"
         << "delta_max_travel 380\n"
         << "delta_limit_enable true\n"
         << "delta_homing_retract_mm 0.5\n"
         << "e_stop_pin 2.15!^\n"
         << "sd_ok true\n"
         << "endstop_debounce_ms 0\n"
         << "soft_endstop.enable true\n"
         << "soft_endstop.x_min -10\n"
         << "soft_endstop.y_min -20\n"
         << "soft_endstop.z_min -30\n";
}

}  // namespace sim::test

#endif
