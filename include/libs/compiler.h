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

#include "sim/lpc_memory_accounting.hpp"

#define LOCATED_IN_AHBSRAM

template <typename Type, std::size_t Capacity>
Type* simulator_config_cache_storage() {
  alignas(Type) static unsigned char storage[Capacity * sizeof(Type)]{};
  sim::lpc_memory::config_cache_storage_acquired();
  return reinterpret_cast<Type*>(storage);
}

#define CONFIG_CACHE_STORAGE(Type, Capacity) simulator_config_cache_storage<Type, Capacity>()

#endif
