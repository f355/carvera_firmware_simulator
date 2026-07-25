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

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include "support/assertions.hpp"
#include "support/memory_config.hpp"

#include "Config.h"
#include "Gcode.h"
#include "Robot.h"
#include "libs/Kernel.h"
#include "modules/tools/zprobe/CartGridStrategy.h"
#include "modules/tools/zprobe/ZProbe.h"
#include "sim/machine_simulator.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;
using sim::test::require_near;

template <typename T>
void write_binary(std::ofstream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_cartesian_grid(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary);
  const std::uint8_t x_points = 2;
  const std::uint8_t y_points = 2;
  const float x_start = 0.0F;
  const float y_start = 0.0F;
  const float x_size = 20.0F;
  const float y_size = 20.0F;
  const std::array<float, 4> heights = {0.0F, 1.0F, 2.0F, 3.0F};

  write_binary(output, x_points);
  write_binary(output, y_points);
  write_binary(output, x_start);
  write_binary(output, y_start);
  write_binary(output, x_size);
  write_binary(output, y_size);
  for (float height : heights) {
    write_binary(output, height);
  }
}

void write_flex_grid(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary);
  const float version = 1.0F;
  const float x_start = 0.0F;
  const std::uint8_t x_points = 3;
  const float x_size = 20.0F;
  const std::array<float, 3> compensation = {0.0F, 0.01F, 0.0F};

  write_binary(output, version);
  write_binary(output, x_start);
  write_binary(output, x_points);
  write_binary(output, x_size);
  for (float value : compensation) {
    write_binary(output, value);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::test::TempSdCard sd("carvera_sim_cart_grid_strategy_test");
  sd.mount();
  Kernel kernel;
  kernel.factory_set->MachineModel = CARVERA_AIR;

  kernel.config = new Config(new MemoryConfigSource({
      "leveling-strategy.rectangular-grid.enable true\n",
      "leveling-strategy.rectangular-grid.size 3\n",
      "leveling-strategy.rectangular-grid.x_size 20\n",
      "leveling-strategy.rectangular-grid.y_size 20\n",
      "leveling-strategy.rectangular-grid.only_by_two_corners true\n",
      "leveling-strategy.rectangular-grid.flex_x_points 3\n",
      "leveling-strategy.rectangular-grid.flex_compensation_always_active false\n",
  }));
  kernel.config->config_cache_load();

  ZProbe probe;
  CartGridStrategy strategy(&probe);
  require(strategy.handleConfig(), "real rectangular-grid configuration should initialize successfully");

  const auto cartesian_path = sd.path() / "cartesian_nm.grid";
  write_cartesian_grid(cartesian_path);
  Gcode load_cartesian("M375", &StreamOutput::NullStream);
  require(strategy.handleGcode(&load_cartesian), "M375 should load the persisted rectangular grid");
  require(static_cast<bool>(kernel.robot->compensationTransform),
          "loading a rectangular grid should enable the robot compensation transform");

  float target[3] = {10.0F, 10.0F, 0.0F};
  kernel.robot->compensationTransform(target, false, false);
  require_near(target[2], 1.5, 1.0e-5, "grid compensation should bilinearly interpolate the four surrounding points");
  kernel.robot->compensationTransform(target, true, false);
  require_near(target[2], 0.0, 1.0e-5, "inverse grid compensation should restore the uncompensated coordinate");

  float outside[3] = {25.0F, 10.0F, 4.0F};
  kernel.robot->compensationTransform(outside, false, false);
  require_near(outside[2], 4.0, 1.0e-6, "grid compensation should not alter moves outside the measured area");

  std::filesystem::remove(cartesian_path);
  Gcode save_cartesian("M374", &StreamOutput::NullStream);
  require(strategy.handleGcode(&save_cartesian), "M374 should save the active rectangular grid");
  require(std::filesystem::exists(cartesian_path), "rectangular-grid save should create the production grid file");

  Gcode delete_cartesian("M374.1", &StreamOutput::NullStream);
  require(strategy.handleGcode(&delete_cartesian), "M374.1 should delete the persisted rectangular grid");
  require(!std::filesystem::exists(cartesian_path), "rectangular-grid delete should remove the production grid file");

  Gcode clear_cartesian("M370", &StreamOutput::NullStream);
  require(strategy.handleGcode(&clear_cartesian), "M370 should clear rectangular-grid compensation");
  require(!kernel.robot->compensationTransform,
          "clearing the only active compensation mode should remove the robot transform");

  const auto flex_path = sd.path() / "flex_compensation.dat";
  write_flex_grid(flex_path);
  Gcode load_flex("M380.3", &StreamOutput::NullStream);
  require(strategy.handleGcode(&load_flex), "M380.3 should load persisted CA1 flex compensation");
  require(kernel.is_flex_compensation_active(), "loading valid flex data should publish active compensation state");
  require(static_cast<bool>(kernel.robot->compensationTransform),
          "loading flex data should enable the shared robot compensation transform");

  float flex_target[3] = {10.0F, 0.0F, -50.0F};
  kernel.robot->compensationTransform(flex_target, false, false);
  require(flex_target[1] != 0.0F && flex_target[2] != -50.0F,
          "CA1 flex compensation should adjust both Y and Z at a measured X position");

  std::filesystem::remove(flex_path);
  Gcode save_flex("M380.2", &StreamOutput::NullStream);
  require(strategy.handleGcode(&save_flex), "M380.2 should save active CA1 flex compensation");
  require(std::filesystem::exists(flex_path), "flex save should create the production compensation file");

  Gcode delete_flex("M380.4", &StreamOutput::NullStream);
  require(strategy.handleGcode(&delete_flex), "M380.4 should delete persisted CA1 flex compensation");
  require(!std::filesystem::exists(flex_path), "flex delete should remove the production compensation file");

  Gcode disable_flex("M380", &StreamOutput::NullStream);
  require(strategy.handleGcode(&disable_flex), "M380 should disable CA1 flex compensation");
  require(!kernel.is_flex_compensation_active(), "disabling flex compensation should publish inactive state");
  require(!kernel.robot->compensationTransform,
          "disabling the only active compensation mode should remove the robot transform");

  return 0;
}
