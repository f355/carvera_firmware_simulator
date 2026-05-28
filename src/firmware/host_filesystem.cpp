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

#include "sim/host_filesystem.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

#include "sim/host_file_shim.h"
#include "sim/i2c_eeprom.hpp"
#include "sim/simulator_context.hpp"

namespace {

constexpr const char* sd_mount_name = "sd";
constexpr const char* eeprom_filename = ".eeprom.bin";

bool write_mode(const char* mode) {
  return mode != nullptr &&
         (std::strchr(mode, 'w') != nullptr || std::strchr(mode, 'a') != nullptr || std::strchr(mode, '+') != nullptr);
}

std::filesystem::path host_path(const char* path) { return sim::host_filesystem::translate(path); }

}  // namespace

namespace sim {

std::map<std::string, std::filesystem::path> HostFilesystem::snapshot_mounts() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return mounts_;
}

void HostFilesystem::copy_mounts_from(const HostFilesystem& other) {
  std::lock_guard<std::mutex> lock(mutex_);
  mounts_ = other.snapshot_mounts();
}

void HostFilesystem::clear_mounts(I2cEepromDevice& eeprom) {
  std::lock_guard<std::mutex> lock(mutex_);
  mounts_.clear();
  eeprom.clear_persistent_file();
}

void HostFilesystem::mount(const std::string& name, const std::filesystem::path& root, I2cEepromDevice& eeprom) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto absolute_root = std::filesystem::absolute(root);
  std::filesystem::create_directories(absolute_root);
  mounts_[name] = absolute_root;
  if (name == sd_mount_name) {
    eeprom.use_persistent_file(absolute_root / eeprom_filename);
  }
}

void HostFilesystem::ensure_mount(const std::string& name, I2cEepromDevice& eeprom) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mounts_.find(name) != mounts_.end()) {
    return;
  }

  const auto root = std::filesystem::absolute(host_filesystem::default_mount_root(name));
  std::filesystem::create_directories(root);
  mounts_[name] = root;
  if (name == sd_mount_name) {
    eeprom.use_persistent_file(root / eeprom_filename);
  }
}

std::filesystem::path HostFilesystem::translate(const char* path) const {
  if (path == nullptr) {
    return {};
  }

  std::string text(path);
  if (text.size() < 2 || text[0] != '/') {
    return std::filesystem::path(text);
  }

  const auto slash = text.find('/', 1);
  const auto mount_name = text.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);

  std::lock_guard<std::mutex> lock(mutex_);
  auto found = mounts_.find(mount_name);
  if (found == mounts_.end()) {
    return std::filesystem::path(text);
  }

  if (slash == std::string::npos) {
    return found->second;
  }

  return found->second / text.substr(slash + 1);
}

bool HostFilesystem::exists(const std::string& path) const {
  std::error_code error;
  return std::filesystem::exists(translate(path.c_str()), error);
}

}  // namespace sim

namespace sim::host_filesystem {

void clear_mounts() { simulator_context::active().host_filesystem().clear_mounts(i2c_eeprom::active()); }

std::filesystem::path default_mount_root(const std::string& name) {
  return std::filesystem::temp_directory_path() / "carvera_firmware_sim" / name;
}

void mount(const std::string& name, const std::filesystem::path& root) {
  simulator_context::active().host_filesystem().mount(name, root, i2c_eeprom::active());
}

void ensure_mount(const std::string& name) {
  simulator_context::active().host_filesystem().ensure_mount(name, i2c_eeprom::active());
}

std::filesystem::path translate(const char* path) {
  return simulator_context::active().host_filesystem().translate(path);
}

bool exists(const std::string& path) { return simulator_context::active().host_filesystem().exists(path); }

}  // namespace sim::host_filesystem

extern "C" FILE* sim_fopen(const char* path, const char* mode) {
  const auto translated = host_path(path);
  if (write_mode(mode)) {
    std::error_code error;
    std::filesystem::create_directories(translated.parent_path(), error);
  }
  return std::fopen(translated.c_str(), mode);
}

extern "C" FILE* sim_freopen(const char* path, const char* mode, FILE* stream) {
  const auto translated = host_path(path);
  if (write_mode(mode)) {
    std::error_code error;
    std::filesystem::create_directories(translated.parent_path(), error);
  }
  return std::freopen(translated.c_str(), mode, stream);
}

extern "C" int sim_remove(const char* path) { return std::remove(host_path(path).c_str()); }

extern "C" int sim_rename(const char* old_path, const char* new_path) {
  const auto translated_new = host_path(new_path);
  std::error_code error;
  std::filesystem::create_directories(translated_new.parent_path(), error);
  return std::rename(host_path(old_path).c_str(), translated_new.c_str());
}

extern "C" DIR* sim_opendir(const char* path) { return ::opendir(host_path(path).c_str()); }

extern "C" int sim_mkdir(const char* path, mode_t mode) {
  (void)mode;
  std::error_code error;
  std::filesystem::create_directories(host_path(path), error);
  if (error) {
    errno = error.value();
    return -1;
  }
  return 0;
}
