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

#ifndef SIMULATOR_SIM_HOST_FILESYSTEM_HPP
#define SIMULATOR_SIM_HOST_FILESYSTEM_HPP

#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace sim {

class I2cEepromDevice;

class HostFilesystem {
 public:
  void clear_mounts(I2cEepromDevice& eeprom);
  void mount(const std::string& name, const std::filesystem::path& root, I2cEepromDevice& eeprom);
  void ensure_mount(const std::string& name, I2cEepromDevice& eeprom);
  void copy_mounts_from(const HostFilesystem& other);
  std::filesystem::path translate(const char* path) const;
  bool exists(const std::string& path) const;

 private:
  std::map<std::string, std::filesystem::path> snapshot_mounts() const;

  mutable std::mutex mutex_;
  std::map<std::string, std::filesystem::path> mounts_;
};

}  // namespace sim

namespace sim::host_filesystem {

void clear_mounts();
void mount(const std::string& name, const std::filesystem::path& root);
void ensure_mount(const std::string& name);
std::filesystem::path default_mount_root(const std::string& name);
std::filesystem::path translate(const char* path);
bool exists(const std::string& path);

}  // namespace sim::host_filesystem

#endif
