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

#ifndef SIMULATOR_TESTS_SUPPORT_DIRECT_ROBOT_CONFIG_HPP
#define SIMULATOR_TESTS_SUPPORT_DIRECT_ROBOT_CONFIG_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace sim::test {

inline std::vector<std::string> direct_robot_config_lines() {
  return {
      "arm_solution cartesian\n",    "alpha_step_pin 1.18\n",   "alpha_dir_pin 1.20\n",        "alpha_en_pin nc\n",
      "alpha_steps_per_mm 10\n",     "alpha_max_rate 6000\n",   "alpha_acceleration 1000\n",   "beta_step_pin 1.19\n",
      "beta_dir_pin 1.21\n",         "beta_en_pin nc\n",        "beta_steps_per_mm 10\n",      "beta_max_rate 6000\n",
      "beta_acceleration 1000\n",    "gamma_step_pin 1.22\n",   "gamma_dir_pin 1.23\n",        "gamma_en_pin nc\n",
      "gamma_steps_per_mm 10\n",     "gamma_max_rate 6000\n",   "gamma_acceleration 1000\n",   "delta_step_pin 1.24\n",
      "delta_dir_pin 1.25\n",        "delta_en_pin nc\n",       "delta_steps_per_mm 10\n",     "delta_max_rate 6000\n",
      "delta_acceleration 1000\n",   "epsilon_step_pin 1.26\n", "epsilon_dir_pin 1.27\n",      "epsilon_en_pin nc\n",
      "epsilon_steps_per_mm 10\n",   "epsilon_max_rate 6000\n", "epsilon_acceleration 1000\n", "acceleration 1000\n",
      "soft_endstop.enable false\n",
  };
}

inline std::string direct_robot_config() {
  std::ostringstream config;
  for (const auto& line : direct_robot_config_lines()) {
    config << line;
  }
  return config.str();
}

inline void write_direct_robot_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream output(root / "config");
  output << direct_robot_config();
}

}  // namespace sim::test

#endif
