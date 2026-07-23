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

#include "test_support.hpp"

#include "Config.h"
#include "libs/Kernel.h"
#include "sim/lpc_memory_constraints.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/system_reset.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::lpc_memory::set_lpc_heap_enabled(true);
  sim::lpc_memory::reset_for_reboot();

  const auto cache = reinterpret_cast<std::uintptr_t>(
      sim::lpc_memory::config_cache_base(sim::lpc_memory::kConfigCacheBytes));
  require(cache == sim::lpc_memory::stack_limit_address() - sim::lpc_memory::kConfigCacheBytes,
          "config cache should sit immediately below StackLimit");
  require(sim::lpc_memory::heap_break_address() < cache,
          "fresh reboot heap break should start below the config cache");
  require(sim::lpc_memory::maximum_heap_address() > cache,
          "maximum heap address should allow growth through the cache window");

  sim::MachineSimulator simulator;
  Kernel kernel;
  kernel.config = new Config(new MemoryConfigSource({"dummy.enable true\n"}));
  kernel.config->config_cache_load(false);
  require(kernel.config->is_config_cache_loaded(), "config cache should load when parse=false");
  require(sim::lpc_memory::config_cache_live(), "config cache object should mark the live window");

  const auto need = static_cast<int>(cache - sim::lpc_memory::heap_break_address() + 64);
  require(need > 0, "heap should still be below cache after a minimal Kernel+Config setup");
  require(reinterpret_cast<std::intptr_t>(sim::lpc_memory::sbrk(need)) != -1,
          "sbrk should be allowed to advance into the config-cache window");
  require(sim::lpc_memory::heap_break_address() > cache, "heap break should pass cache_start");

  sim::system_reset::consume_requested();
  kernel.config->config_cache_clear();
  require(sim::system_reset::consume_requested(),
          "config_cache_clear should FATAL/reset when `_sbrk(0)` is past cache_start");
  require(!sim::lpc_memory::config_cache_live(), "clearing the cache should end the live window");

  sim::lpc_memory::set_lpc_heap_enabled(false);
  return 0;
}
