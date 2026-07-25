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
#include <string>

#include "support/assertions.hpp"

#include "libs/Kernel.h"
#include "sim/lpc_memory_constraints.hpp"
#include "sim/system_reset.hpp"
#include "support/booted_runtime.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::require;

template <typename T>
void write_binary(std::ofstream& output, const T& value) {
  output.write(reinterpret_cast<const char*>(&value), sizeof(value));
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

std::string ca1_grid_config(bool always_active) {
  return std::string("sd_ok true\n") +
         "home_on_boot false\n"
         "leveling-strategy.rectangular-grid.enable true\n"
         "leveling-strategy.rectangular-grid.size 3\n"
         "leveling-strategy.rectangular-grid.x_size 20\n"
         "leveling-strategy.rectangular-grid.y_size 20\n"
         "leveling-strategy.rectangular-grid.only_by_two_corners true\n"
         "leveling-strategy.rectangular-grid.flex_x_points 3\n"
         "leveling-strategy.rectangular-grid.flex_compensation_always_active " +
         std::string(always_active ? "true" : "false") + "\n";
}

void run_case(bool always_active) {
  sim::lpc_memory::set_lpc_heap_enabled(true);

  sim::test::TempSdCard sd(always_active ? "carvera_sim_flex_heap_boot_on" : "carvera_sim_flex_heap_boot_off");
  sd.write_config_txt(ca1_grid_config(always_active));
  write_flex_grid(sd.path() / "flex_compensation.dat");

  sim::FactorySettings factory;
  factory.machine_model = sim::MachineModel::CarveraAirCA1;
  sim::PersistentMachineConfig persistent;
  persistent.mounts.push_back({"sd", sd.path().string()});

  sim::system_reset::consume_requested();
  const auto reboots_before = sim::lpc_memory::firmware_reboot_count();
  sim::test::BootedRuntime runtime(persistent, factory);

  const bool reset_seen = sim::system_reset::consume_requested();
  const bool rebooted = sim::lpc_memory::firmware_reboot_count() > reboots_before;

  // Firmware loads always-active flex only after config_cache_clear(), so boot
  // must complete without a heap↔cache FATAL whether or not always_active is set.
  require(runtime.kernel().conveyor != nullptr, "flex boot should finish with LPC heap enabled");
  require(!reset_seen && !rebooted, "deferred flex load should not FATAL/reboot during boot");

  sim::lpc_memory::set_lpc_heap_enabled(false);
}

}  // namespace

int main() {
  run_case(/*always_active=*/false);
  run_case(/*always_active=*/true);
  return 0;
}
