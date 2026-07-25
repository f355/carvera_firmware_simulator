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

#include <vector>

#include "Config.h"
#include "ConfigValue.h"
#include "checksumm.h"
#include "support/assertions.hpp"
#include "support/memory_config.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  auto* source = new MemoryConfigSource({
      "alpha 42\n",
      "module.test.enable true\n",
  });
  Config config(source);

  config.config_cache_load();
  require(config.is_config_cache_loaded(), "Config should load a real cache");
  require(config.value(CHECKSUM("alpha"))->as_number() == 42.0F, "Config should parse numeric values");
  require(config.value(CHECKSUM("missing"))->as_number(7.0F) == 7.0F, "Missing values should use defaults");

  std::vector<uint16_t> modules;
  config.get_module_list(&modules, CHECKSUM("module"));
  require(modules.size() == 1, "Config should collect enabled module names");
  require(modules[0] == CHECKSUM("test"), "Config should collect the middle checksum for modules");

  config.config_cache_clear();
  require(!config.is_config_cache_loaded(), "Config should clear the cache");

  return 0;
}
