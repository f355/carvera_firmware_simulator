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

#ifndef SIMULATOR_TESTS_SUPPORT_C1_ATC_CONFIG_HPP
#define SIMULATOR_TESTS_SUPPORT_C1_ATC_CONFIG_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace sim::test {

struct C1AtcConfigOptions {
  bool include_soft_limit_coordinates = true;
  bool include_detector_motion_limits = true;
  bool include_cartesian_homing_retracts = true;
  bool include_atc_home_pin = true;
};

inline std::string c1_atc_config(const C1AtcConfigOptions& options = {}) {
  std::ostringstream config;
  config << "arm_solution cartesian\n"
         << "alpha_step_pin 1.28\n"
         << "alpha_dir_pin 1.29\n"
         << "alpha_en_pin nc\n"
         << "alpha_steps_per_mm 200\n"
         << "alpha_max_rate 3000\n"
         << "alpha_acceleration 150\n"
         << "alpha_max_endstop 0.25^\n"
         << "alpha_homing_direction home_to_max\n"
         << "alpha_max -2\n"
         << "alpha_max_travel 400\n"
         << "alpha_limit_enable false\n";
  if (options.include_cartesian_homing_retracts) {
    config << "alpha_homing_retract_mm 2\n";
  }
  config << "beta_step_pin 1.26\n"
         << "beta_dir_pin 1.27\n"
         << "beta_en_pin nc\n"
         << "beta_steps_per_mm 200\n"
         << "beta_max_rate 3000\n"
         << "beta_acceleration 150\n"
         << "beta_max_endstop 1.4^\n"
         << "beta_homing_direction home_to_max\n"
         << "beta_max -2\n"
         << "beta_max_travel 300\n"
         << "beta_limit_enable false\n";
  if (options.include_cartesian_homing_retracts) {
    config << "beta_homing_retract_mm 2\n";
  }
  config << "gamma_step_pin 1.24\n"
         << "gamma_dir_pin 1.25\n"
         << "gamma_en_pin nc\n"
         << "gamma_steps_per_mm 200\n"
         << "gamma_max_rate 3000\n"
         << "gamma_acceleration 150\n"
         << "gamma_max_endstop 1.8^\n"
         << "gamma_homing_direction home_to_max\n"
         << "gamma_max -2\n"
         << "gamma_max_travel 150\n"
         << "gamma_limit_enable false\n";
  if (options.include_cartesian_homing_retracts) {
    config << "gamma_homing_retract_mm 2\n";
  }
  config << "delta_step_pin 1.18\n"
         << "delta_dir_pin 1.20\n"
         << "delta_en_pin nc\n"
         << "delta_steps_per_mm 26.666667\n"
         << "delta_max_rate 1000\n"
         << "delta_acceleration 1000\n"
         << "epsilon_step_pin 1.21\n"
         << "epsilon_dir_pin 1.23\n"
         << "epsilon_en_pin nc\n"
         << "epsilon_steps_per_mm 200\n"
         << "epsilon_max_rate 1000\n"
         << "epsilon_acceleration 1000\n"
         << "endstop_debounce_ms 0\n"
         << "sd_ok true\n"
         << "watchdog_timeout 0\n"
         << "soft_endstop.enable false\n";
  if (options.include_soft_limit_coordinates) {
    config << "soft_endstop.x_min -371\n"
           << "soft_endstop.y_min -250\n"
           << "soft_endstop.z_min -135\n";
  }
  config << "zprobe.enable true\n"
         << "zprobe.probe_pin 2.6\n"
         << "zprobe.calibrate_pin 0.5^\n"
         << "zprobe.debounce_ms 0\n"
         << "switch.toolsensor.enable true\n"
         << "switch.toolsensor.output_pin 1.22\n"
         << "switch.toolsensor.output_type digital\n";
  if (options.include_atc_home_pin) {
    config << "atc.homing_endstop_pin 1.0^\n";
  }
  config << "atc.homing_max_travel_mm 8\n"
         << "atc.homing_retract_mm 0.4\n"
         << "atc.homing_rate_mm_s 2\n"
         << "atc.detector.detect_pin 0.20^\n";
  if (options.include_detector_motion_limits) {
    config << "atc.detector.detect_rate_mm_s 20\n"
           << "atc.detector.detect_travel_mm 5\n";
  }
  config << "atc.detector.enable true\n"
         << "atc.probe.fast_rate_mm_m 500\n"
         << "atc.probe.slow_rate_mm_m 100\n"
         << "atc.probe.retract_mm 2\n"
         << "coordinate.anchor1_x -360.158\n"
         << "coordinate.anchor1_y -234.568\n"
         << "coordinate.toolrack_offset_x 356\n"
         << "coordinate.toolrack_offset_y 0\n"
         << "coordinate.toolrack_z -112.5\n"
         << "coordinate.clearance_z -3\n";
  return config.str();
}

inline void write_c1_atc_config(const std::filesystem::path& root, const C1AtcConfigOptions& options = {}) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << c1_atc_config(options);
}

}  // namespace sim::test

#endif
