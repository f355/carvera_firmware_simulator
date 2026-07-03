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
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

#include "libs/FirmwareFileSystem.h"
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

std::string host_path_string(const char* path) { return host_path(path).string(); }

}  // namespace

namespace sim {

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

namespace fwfs {

FILE* fopen(const char* path, const char* mode) {
  const auto translated = host_path(path);
  if (write_mode(mode)) {
    std::error_code error;
    std::filesystem::create_directories(translated.parent_path(), error);
  }
  return std::fopen(translated.string().c_str(), mode);
}

FILE* freopen(const char* path, const char* mode, FILE* stream) {
  const auto translated = host_path(path);
  if (write_mode(mode)) {
    std::error_code error;
    std::filesystem::create_directories(translated.parent_path(), error);
  }
  return std::freopen(translated.string().c_str(), mode, stream);
}

int fclose(FILE* stream) { return std::fclose(stream); }

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) { return std::fread(ptr, size, count, stream); }

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
  return std::fwrite(ptr, size, count, stream);
}

int fprintf(FILE* stream, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int result = std::vfprintf(stream, format, args);
  va_end(args);
  return result;
}

char* fgets(char* str, int count, FILE* stream) { return std::fgets(str, count, stream); }

int fputs(const char* str, FILE* stream) { return std::fputs(str, stream); }

int fseek(FILE* stream, long offset, int origin) { return std::fseek(stream, offset, origin); }

long ftell(FILE* stream) { return std::ftell(stream); }

int fgetc(FILE* stream) { return std::fgetc(stream); }

int fputc(int ch, FILE* stream) { return std::fputc(ch, stream); }

int fgetpos(FILE* stream, fpos_t* pos) { return std::fgetpos(stream, pos); }

int fsetpos(FILE* stream, const fpos_t* pos) { return std::fsetpos(stream, pos); }

int sim_fgetpos_as_long(FILE* stream, long* pos) {
  const long value = std::ftell(stream);
  if (value < 0) {
    return -1;
  }
  *pos = value;
  return 0;
}

int sim_fsetpos_as_long(FILE* stream, const long* pos) { return std::fseek(stream, *pos, SEEK_SET); }

int feof(FILE* stream) { return std::feof(stream); }

int remove(const char* path) {
  const auto translated = host_path_string(path);
  return std::remove(translated.c_str());
}

int rename(const char* old_path, const char* new_path) {
  const auto translated_new = host_path(new_path);
  std::error_code error;
  std::filesystem::create_directories(translated_new.parent_path(), error);
  const auto translated_old = host_path_string(old_path);
  return std::rename(translated_old.c_str(), translated_new.string().c_str());
}

DIR* opendir(const char* path) {
  const auto translated = host_path_string(path);
  return ::opendir(translated.c_str());
}

int mkdir(const char* path, int mode) {
  (void)mode;
  std::error_code error;
  std::filesystem::create_directories(host_path(path), error);
  if (error) {
    errno = error.value();
    return -1;
  }
  return 0;
}

}  // namespace fwfs
