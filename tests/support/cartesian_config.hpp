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

#ifndef SIMULATOR_TESTS_SUPPORT_CARTESIAN_CONFIG_HPP
#define SIMULATOR_TESTS_SUPPORT_CARTESIAN_CONFIG_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace sim::test {

enum class TestProtocol {
  Makera,
  Smoothie,
};

struct CartesianConfigOptions {
  bool include_rotary_axes = false;
  bool include_probe_inputs = false;
  bool sd_ok = true;
  bool soft_endstop = false;
  TestProtocol protocol = TestProtocol::Makera;
  std::string extra;
};

inline std::string cartesian_config(const CartesianConfigOptions& options = {}) {
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
         << "alpha_max 0\n"
         << "alpha_max_travel 20\n"
         << "alpha_homing_retract_mm 5\n"
         << "beta_step_pin 1.26\n"
         << "beta_dir_pin 1.27\n"
         << "beta_en_pin nc\n"
         << "beta_steps_per_mm 200\n"
         << "beta_max_rate 3000\n"
         << "beta_acceleration 150\n"
         << "beta_max_endstop 1.4^\n"
         << "beta_homing_direction home_to_max\n"
         << "beta_max 0\n"
         << "beta_max_travel 20\n"
         << "beta_homing_retract_mm 5\n"
         << "gamma_step_pin 1.24\n"
         << "gamma_dir_pin 1.25\n"
         << "gamma_en_pin nc\n"
         << "gamma_steps_per_mm 200\n"
         << "gamma_max_rate 3000\n"
         << "gamma_acceleration 150\n"
         << "gamma_max_endstop 1.8^\n"
         << "gamma_homing_direction home_to_max\n"
         << "gamma_max 0\n"
         << "gamma_max_travel 20\n"
         << "gamma_homing_retract_mm 5\n";

  if (options.include_rotary_axes) {
    config << "delta_step_pin 1.18\n"
           << "delta_dir_pin 1.20\n"
           << "delta_en_pin nc\n"
           << "delta_steps_per_mm 26.666667\n"
           << "delta_max_rate 1800\n"
           << "delta_acceleration 360\n"
           << "epsilon_step_pin 1.21\n"
           << "epsilon_dir_pin 1.23\n"
           << "epsilon_en_pin nc\n"
           << "epsilon_steps_per_mm 43200\n"
           << "epsilon_max_rate 100\n"
           << "epsilon_acceleration 10\n";
  }

  config << "endstop_debounce_ms 0\n";
  if (options.include_probe_inputs) {
    config << "zprobe.enable true\n"
           << "zprobe.probe_pin 2.6\n"
           << "zprobe.calibrate_pin 0.5\n"
           << "zprobe.debounce_ms 0\n";
  }
  if (options.sd_ok) {
    config << "sd_ok true\n";
  }
  config << "protocol " << (options.protocol == TestProtocol::Makera ? "makera" : "smoothie") << "\n";
  config << "soft_endstop.enable " << (options.soft_endstop ? "true" : "false") << "\n";
  config << options.extra;
  return config.str();
}

inline void write_cartesian_config(const std::filesystem::path& root, const CartesianConfigOptions& options = {}) {
  std::filesystem::create_directories(root);
  std::ofstream output(root / "config");
  output << cartesian_config(options);
}

}  // namespace sim::test

#endif
