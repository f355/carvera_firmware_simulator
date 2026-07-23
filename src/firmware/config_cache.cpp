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

#include "ConfigCache.h"

#include <cstring>

#include "libs/Kernel.h"
#include "libs/StreamOutput.h"
#include "libs/StreamOutputPool.h"
#include "sim/lpc_memory_constraints.hpp"

ConfigCache::ConfigCache() {
  // Place the store in the gap between heap and stack — no heap allocation.
  store = CONFIG_CACHE_STORAGE(ConfigValue, CONFIG_CACHE_CAPACITY);
  count = 0;
  // While the cache is live, host new/fopen advance the LPC `_sbrk` watermark
  // (scaled). That is the window where heap↔cache collisions matter on device.
  sim::lpc_memory::note_config_cache_live(true);
}

ConfigCache::~ConfigCache() {
  clear();
  sim::lpc_memory::note_config_cache_live(false);
}

void ConfigCache::clear() { count = 0; }

void ConfigCache::pop() {
  if (count > 0) {
    count--;
  }
}

ConfigValue* ConfigCache::replace_or_push_back(const ConfigValue& new_value) {
  for (uint16_t i = 0; i < count; i++) {
    if (memcmp(new_value.check_sums, store[i].check_sums, sizeof(store[i].check_sums)) == 0) {
      store[i] = new_value;
      return &store[i];
    }
  }

  if (count < CONFIG_CACHE_CAPACITY) {
    store[count] = new_value;
    return &store[count++];
  }

  THEKERNEL->streams->printf("ERROR: config cache overflow (capacity=%u), dropping key %04X:%04X:%04X\n",
                             (unsigned)CONFIG_CACHE_CAPACITY, new_value.check_sums[0], new_value.check_sums[1],
                             new_value.check_sums[2]);
  THEKERNEL->set_config_load_error(true);
  return NULL;
}

ConfigValue* ConfigCache::lookup(const uint16_t* check_sums) {
  for (uint16_t i = 0; i < count; i++) {
    if (memcmp(check_sums, store[i].check_sums, sizeof(store[i].check_sums)) == 0) {
      return &store[i];
    }
  }
  return NULL;
}

void ConfigCache::collect(uint16_t family, uint16_t cs, vector<uint16_t>* list) {
  for (uint16_t i = 0; i < count; i++) {
    if (store[i].check_sums[2] == cs && store[i].check_sums[0] == family) {
      list->push_back(store[i].check_sums[1]);
    }
  }
}

void ConfigCache::dump(StreamOutput* stream) {
  for (uint16_t i = 0; i < count; i++) {
    stream->printf("%3d - %04X %04X %04X : '%s'\n", i + 1, store[i].check_sums[0], store[i].check_sums[1],
                   store[i].check_sums[2], store[i].value);
  }
}
