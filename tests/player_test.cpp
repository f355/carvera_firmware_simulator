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
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "support/assertions.hpp"
#include "support/memory_config.hpp"

#define private public
#include "modules/utils/player/Player.h"
#undef private

#include "Config.h"
#include "Gcode.h"
#include "PlayerPublicAccess.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

constexpr std::string_view kDecompressedGcode =
    "G90\n"
    "G0 X0 Y0 Z5\n"
    "G1 X10 Y10 F600\n"
    "G1 X20 Y10 F600\n"
    "G1 X20 Y20 F600\n"
    "M5\n";

// Production .lz upload format: big-endian block size, QuickLZ 1.5 level-3 data,
// then a big-endian 16-bit additive checksum of the decompressed bytes.
constexpr std::array<std::uint8_t, 58> kQuickLzFile = {
    0x00, 0x00, 0x00, 0x34, 0x4d, 0x34, 0x43, 0x00, 0x00, 0x60, 0xec, 0x47, 0x39, 0x30, 0x0a,
    0x47, 0x30, 0x20, 0x58, 0x30, 0x20, 0x59, 0x30, 0x20, 0x5a, 0x35, 0x0a, 0x47, 0x31, 0x20,
    0x58, 0x31, 0x34, 0x10, 0x46, 0x36, 0x30, 0x70, 0x40, 0x32, 0x47, 0x08, 0x00, 0x10, 0x00,
    0x00, 0x00, 0x80, 0x46, 0x36, 0x30, 0x30, 0x0a, 0x4d, 0x35, 0x0a, 0x0d, 0x5d,
};

struct SpindleCommand {
  unsigned int code;
  float rpm;
};

class SpindleCommandCapture : public Module {
 public:
  void on_gcode_received(void* argument) override {
    auto* gcode = static_cast<Gcode*>(argument);
    if (gcode->has_m && (gcode->m == 3 || gcode->m == 4 || gcode->m == 5)) {
      commands.push_back({gcode->m, gcode->has_letter('S') ? gcode->get_value('S') : 0.0F});
    }
  }

  std::vector<SpindleCommand> commands;
};

}  // namespace

int main() {
  sim::MachineSimulator simulator;
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

  {
    std::ofstream compressed(root / "gcodes" / "demo.lz", std::ios::binary);
    compressed.write(reinterpret_cast<const char*>(kQuickLzFile.data()),
                     static_cast<std::streamsize>(kQuickLzFile.size()));
  }
  require(player.decompress("/sd/gcodes/demo.lz", "/sd/gcodes/decompressed.cnc", kQuickLzFile.size(),
                            &StreamOutput::NullStream) == 1,
          "Player should decompress the QuickLZ container used for uploaded G-code files");

  std::ifstream decompressed(root / "gcodes" / "decompressed.cnc", std::ios::binary);
  const std::string decompressed_gcode((std::istreambuf_iterator<char>(decompressed)),
                                       std::istreambuf_iterator<char>());
  require(decompressed_gcode == kDecompressedGcode,
          "Player should preserve G-code contents while decompressing an uploaded file");

  SpindleCommandCapture spindle_commands;
  kernel.register_for_event(ON_GCODE_RECEIVED, &spindle_commands);

  Gcode spindle_start("M4 S7200", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &spindle_start);
  spindle_commands.commands.clear();

  player.save_and_stop_spindle_on_suspend();
  require(spindle_commands.commands.size() == 1 && spindle_commands.commands[0].code == 5,
          "suspending should stop a spindle that was running during file playback");

  spindle_commands.commands.clear();
  player.restore_spindle_on_resume();
  require(spindle_commands.commands.size() == 1 && spindle_commands.commands[0].code == 4,
          "resuming should restore the saved spindle direction");
  require(spindle_commands.commands[0].rpm == 7200.0F, "resuming should restore the saved spindle speed");

  return 0;
}
