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

#include <filesystem>

#include "sim/host_filesystem.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"

int main() {
  using sim::test::require;

  std::filesystem::path first_path;
  std::filesystem::path second_path;

  {
    sim::test::TempSdCard first("carvera_sim_duplicate_name_sdcard_test");
    sim::test::TempSdCard second("carvera_sim_duplicate_name_sdcard_test");
    first_path = first.path();
    second_path = second.path();

    require(first_path != second_path, "same-name TempSdCard instances should use unique directories");
    first.write_config_txt("# first\n");
    second.write_config_txt("# second\n");
    require((first_path / "config.txt").string() != (second_path / "config.txt").string(),
            "same-name TempSdCard instances should not overwrite each other");
  }

  require(!std::filesystem::exists(first_path), "first TempSdCard should clean up its directory");
  require(!std::filesystem::exists(second_path), "second TempSdCard should clean up its directory");

  {
    sim::test::TempSdCard mounted("carvera_sim_mounted_sdcard_test");
    mounted.write_config_txt("# mounted\n");
    mounted.mount();
    require(sim::host_filesystem::exists("/sd/config.txt"), "mounted TempSdCard should expose files through /sd");
  }
  require(!sim::host_filesystem::exists("/sd/config.txt"), "mounted TempSdCard should clear /sd on destruction");

  return 0;
}
