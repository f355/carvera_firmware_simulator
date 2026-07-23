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

#ifndef SIMULATOR_COMPILER_H
#define SIMULATOR_COMPILER_H

#include <cstddef>

#define LOCATED_IN_AHBSRAM

namespace sim::lpc_memory {
bool lpc_heap_enabled();
void* config_cache_base(std::size_t bytes);
}  // namespace sim::lpc_memory

// Match device CONFIG_CACHE_STORAGE: place the store at StackLimit - N when the
// LPC main-SRAM model is enabled; otherwise keep a disconnected host buffer.
template <typename Type, std::size_t Capacity>
Type* simulator_config_cache_storage() {
  constexpr std::size_t bytes = Capacity * sizeof(Type);
  if (sim::lpc_memory::lpc_heap_enabled()) {
    return reinterpret_cast<Type*>(sim::lpc_memory::config_cache_base(bytes));
  }
  alignas(Type) static unsigned char storage[bytes]{};
  return reinterpret_cast<Type*>(storage);
}

#define CONFIG_CACHE_STORAGE(Type, Capacity) simulator_config_cache_storage<Type, Capacity>()

#endif
