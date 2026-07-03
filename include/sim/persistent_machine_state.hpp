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

#ifndef SIMULATOR_SIM_PERSISTENT_MACHINE_STATE_HPP
#define SIMULATOR_SIM_PERSISTENT_MACHINE_STATE_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "sim/host_filesystem.hpp"
#include "sim/i2c_eeprom.hpp"

namespace sim {

struct FilesystemMount {
  std::string name;
  std::filesystem::path host_root;
};

struct PersistentMachineConfig {
  std::vector<FilesystemMount> mounts;
};

class PersistentMachineState {
 public:
  PersistentMachineState() = default;
  explicit PersistentMachineState(const PersistentMachineConfig& config);

  void clear_mounts();
  void mount(const std::string& name, const std::filesystem::path& host_root);
  void ensure_mount(const std::string& name);

  I2cEepromDevice& eeprom() { return eeprom_; }
  const I2cEepromDevice& eeprom() const { return eeprom_; }

  HostFilesystem& host_filesystem() { return host_filesystem_; }
  const HostFilesystem& host_filesystem() const { return host_filesystem_; }

 private:
  I2cEepromDevice eeprom_{};
  HostFilesystem host_filesystem_{};
};

}  // namespace sim

#endif
