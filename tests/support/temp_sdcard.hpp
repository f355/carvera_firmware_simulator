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

#ifndef SIMULATOR_TESTS_SUPPORT_TEMP_SDCARD_HPP
#define SIMULATOR_TESTS_SUPPORT_TEMP_SDCARD_HPP

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "sim/host_filesystem.hpp"
#include "sim/persistent_machine_state.hpp"

#include <unistd.h>

namespace sim::test {

inline std::filesystem::path make_unique_temp_sdcard_root(const std::string& name) {
  static std::atomic_uint64_t sequence{0};
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto suffix =
        std::to_string(getpid()) + "-" + std::to_string(now) + "-" + std::to_string(sequence.fetch_add(1));
    const auto root = std::filesystem::temp_directory_path() / (name + "-" + suffix);
    std::error_code error;
    if (std::filesystem::create_directories(root, error)) {
      return root;
    }
  }
  throw std::runtime_error("failed to create unique temporary SD card directory");
}

inline PersistentMachineConfig persistent_sd_config(const std::filesystem::path& root) {
  return PersistentMachineConfig{.mounts = {{"sd", root}}};
}

class TempDirectory {
 public:
  explicit TempDirectory(const std::string& name) : root_(make_unique_temp_sdcard_root(name)) {}

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;

  const std::filesystem::path& path() const { return root_; }

  PersistentMachineConfig persistent_config() const { return persistent_sd_config(root_); }

 private:
  std::filesystem::path root_;
};

class TempSdCard {
 public:
  explicit TempSdCard(const std::string& name) : root_(make_unique_temp_sdcard_root(name)) {}

  ~TempSdCard() {
    if (mounted_) {
      host_filesystem::clear_mounts();
    }
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  TempSdCard(const TempSdCard&) = delete;
  TempSdCard& operator=(const TempSdCard&) = delete;

  const std::filesystem::path& path() const { return root_; }

  PersistentMachineConfig persistent_config() const { return persistent_sd_config(root_); }

  void mount() {
    host_filesystem::clear_mounts();
    host_filesystem::mount("sd", root_);
    mounted_ = true;
  }

  void write_config_txt(const std::string& text) const { write("config.txt", text); }

  void write_config(const std::string& text) const { write("config", text); }

  void write(const std::filesystem::path& relative_path, const std::string& text) const {
    const auto path = root_ / relative_path;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << text;
  }

 private:
  std::filesystem::path root_;
  bool mounted_{false};
};

}  // namespace sim::test

#endif
