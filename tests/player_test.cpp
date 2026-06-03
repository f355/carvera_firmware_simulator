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
#include <fstream>

#include "test_support.hpp"

#define private public
#include "modules/utils/player/Player.h"
#undef private

#include "Config.h"
#include "Gcode.h"
#include "PlayerPublicAccess.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "sim/host_filesystem.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_player_test_sd");
  const auto& root = temp_root.path();
  std::filesystem::create_directories(root / "gcodes");
  {
    std::ofstream file(root / "gcodes" / "demo.cnc");
    file << "; first line\n";
    file << "; second line\n";
  }
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root.string());

  Kernel kernel;
  kernel.config = new Config(new MemoryConfigSource({
      "home_on_boot false\n",
      "on_boot_gcode_enable false\n",
  }));
  kernel.config->config_cache_load();

  Player player;
  player.on_module_loaded();

  Gcode select_file("M23 demo", kernel.streams, true, 1);
  player.on_gcode_received(&select_file);
  Gcode start_file("M24", kernel.streams, true, 2);
  player.on_gcode_received(&start_file);

  require(player.playing_file, "Player should enter playing state after opening a file");
  player.on_main_loop(nullptr);
  require(player.played_lines == 1, "Player should feed one file line per main loop");

  pad_progress* progress = nullptr;
  require(PublicData::get_value(player_checksum, get_progress_checksum, &progress),
          "Player should publish progress through PublicData while playing");
  require(progress != nullptr, "Player progress request should return a progress payload");
  require(progress->parsed_lines == 1, "Player progress should expose parsed line count");
  require(progress->is_playing, "Player progress should report active playback");

  player.on_main_loop(nullptr);
  require(player.played_lines == 2, "Player should feed the second file line on the next main loop");
  player.on_main_loop(nullptr);

  require(!player.playing_file, "Player should stop after reaching EOF");
  require(player.has_last_progress, "Player should preserve final progress after EOF");
  require(player.last_played_lines == 2, "Player final progress should retain played line count");
  progress = nullptr;
  require(PublicData::get_value(player_checksum, get_progress_checksum, &progress),
          "Player should publish last progress after EOF");
  require(progress != nullptr, "Player last progress request should return a progress payload");
  require(!progress->is_playing, "Player last progress should report inactive playback after EOF");
  require(progress->played_lines == 2, "Player last progress should retain final line count");

  return 0;
}
