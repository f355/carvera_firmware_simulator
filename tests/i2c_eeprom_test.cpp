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

#include "mbed.h"
#include "sim/host_filesystem.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/machine_simulator.hpp"
#include "support/temp_sdcard.hpp"
#include "support/assertions.hpp"

using sim::test::require;


int main() {
  sim::MachineSimulator simulator;
  sim::test::TempDirectory temp_root("carvera_sim_i2c_eeprom_test");
  const auto& root = temp_root.path();
  std::filesystem::create_directories(root);

  sim::I2cEepromConfig config;
  config.size_bytes = 128;
  config.page_size = 32;
  config.fill = 0xff;
  sim::i2c_eeprom::configure(config);

  mbed::I2C i2c(P0_27, P0_28);
  i2c.frequency(200000);

  i2c.start();
  require(i2c.write(0xa0) == 0, "EEPROM should acknowledge write device address");
  require(i2c.write(0x00) == 0, "EEPROM should acknowledge high address byte");
  require(i2c.write(0x20) == 0, "EEPROM should acknowledge low address byte");
  require(i2c.write(0x12) == 0, "EEPROM should acknowledge first data byte");
  require(i2c.write(0x34) == 0, "EEPROM should acknowledge second data byte");
  i2c.stop();

  i2c.start();
  require(i2c.write(0xa0) == 0, "EEPROM should acknowledge read address setup");
  require(i2c.write(0x00) == 0, "EEPROM should acknowledge high read address byte");
  require(i2c.write(0x20) == 0, "EEPROM should acknowledge low read address byte");
  i2c.start();
  require(i2c.write(0xa1) == 0, "EEPROM should acknowledge read device address");

  require(i2c.read(1) == 0x12, "EEPROM should return the first byte at the selected address");
  require(i2c.read(1) == 0x34, "EEPROM should auto-increment sequential reads");
  i2c.stop();

  i2c.start();
  require(i2c.write(0xa0) == 0, "EEPROM should acknowledge page-wrap write device address");
  require(i2c.write(0x00) == 0, "EEPROM should acknowledge page-wrap high address byte");
  require(i2c.write(0x3f) == 0, "EEPROM should acknowledge page-wrap low address byte");
  require(i2c.write(0xaa) == 0, "EEPROM should write the last byte of a page");
  require(i2c.write(0xbb) == 0, "EEPROM should wrap writes within the selected page");
  i2c.stop();

  require(sim::i2c_eeprom::read(0x3f) == 0xaa, "EEPROM page write should update the selected byte");
  require(sim::i2c_eeprom::read(0x20) == 0xbb, "EEPROM page write should wrap within the page");

  require(sim::i2c_eeprom::read(0x7f) == 0xff, "EEPROM reset/config fill value should be readable");

  const auto eeprom_path = root / "eeprom.bin";
  sim::i2c_eeprom::clear_persistent_file();
  sim::i2c_eeprom::configure(config);
  require(!sim::i2c_eeprom::use_persistent_file(eeprom_path), "new EEPROM backing file should start empty");
  sim::i2c_eeprom::write(0x22, 0x5a);
  require(std::filesystem::file_size(eeprom_path) == config.size_bytes,
          "EEPROM backing file should mirror device size");

  sim::i2c_eeprom::clear_persistent_file();
  sim::i2c_eeprom::configure(config);
  require(sim::i2c_eeprom::read(0x22) == 0xff, "unbacked EEPROM should not keep old in-memory state");
  require(sim::i2c_eeprom::use_persistent_file(eeprom_path), "existing EEPROM backing file should load");
  require(sim::i2c_eeprom::read(0x22) == 0x5a, "EEPROM backing file should preserve written bytes");

  sim::host_filesystem::clear_mounts();
  sim::i2c_eeprom::clear_persistent_file();
  sim::host_filesystem::mount("sd", root / "sd");
  const auto persistent_file = sim::i2c_eeprom::persistent_file();
  require(persistent_file.has_value(), "mounting /sd should bind the EEPROM backing file");
  require(persistent_file->filename() == ".eeprom.bin", "EEPROM backing file should live beside the mounted SD data");

  return 0;
}
