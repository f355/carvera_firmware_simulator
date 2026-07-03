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

#include <dirent.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

#include "libs/FirmwareFileSystem.h"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "support/temp_sdcard.hpp"
#include "utils.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::test::TempDirectory temp_root("carvera_sim_sdcard_test");
  const auto& root = temp_root.path();
  std::filesystem::create_directories(root);

  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);

  FILE* writer = fwfs::fopen("/sd/config.txt", "w");
  require(writer != nullptr, "fopen('/sd/...', 'w') should create a host-backed SD file");
  fwfs::fputs("alpha 42\n", writer);
  fwfs::fclose(writer);

  require(file_exists("/sd/config.txt"), "file_exists() should see host-backed SD files");

  FILE* reader = fwfs::fopen("/sd/config.txt", "r");
  require(reader != nullptr, "fopen('/sd/...', 'r') should read host-backed SD files");
  char buffer[32]{};
  require(fwfs::fgets(buffer, sizeof(buffer), reader) != nullptr, "SD file should have readable contents");
  fwfs::fclose(reader);
  require(std::strcmp(buffer, "alpha 42\n") == 0, "SD file contents should round-trip");

  require(fwfs::rename("/sd/config.txt", "/sd/config-renamed.txt") == 0, "rename() should work within SD mount");
  require(!file_exists("/sd/config.txt"), "old SD filename should be gone after rename");
  require(file_exists("/sd/config-renamed.txt"), "new SD filename should exist after rename");

  require(fwfs::mkdir("/sd/gcodes", 0) == 0, "mkdir(..., 0) should create host-backed SD directories");
  DIR* dir = fwfs::opendir("/sd/gcodes");
  require(dir != nullptr, "opendir() should open host-backed SD directories");
  closedir(dir);

  require(fwfs::remove("/sd/config-renamed.txt") == 0, "remove() should delete host-backed SD files");
  require(!file_exists("/sd/config-renamed.txt"), "removed SD file should be gone");
  return 0;
}
