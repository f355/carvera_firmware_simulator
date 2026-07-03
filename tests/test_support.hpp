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

#ifndef CARVERA_SIMULATOR_TEST_SUPPORT_HPP
#define CARVERA_SIMULATOR_TEST_SUPPORT_HPP

#include <string>
#include <utility>
#include <vector>

#include "ConfigCache.h"
#include "ConfigSource.h"
#include "checksumm.h"
#include "support/assertions.hpp"

namespace sim::test {

class MemoryConfigSource : public ConfigSource {
 public:
  explicit MemoryConfigSource(std::vector<std::string> lines) : lines_(std::move(lines)) {
    name_checksum = CHECKSUM("memory");
  }

  void transfer_values_to_cache(ConfigCache* cache) override {
    for (const auto& line : lines_) {
      process_line_from_ascii_config(line, cache);
    }
  }

  bool is_named(uint16_t checksum) override { return checksum == name_checksum; }
  bool write(std::string, std::string) override { return false; }
  std::string read(uint16_t[3]) override { return ""; }

 private:
  std::vector<std::string> lines_;
};

}  // namespace sim::test

#endif
