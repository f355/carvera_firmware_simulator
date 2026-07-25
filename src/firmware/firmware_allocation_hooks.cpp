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

#include <cstdlib>
#include <new>

#include "sim/lpc_memory_accounting.hpp"

#if defined(__GNUC__) || defined(__clang__)
#define SIM_NO_INSTRUMENT __attribute__((no_instrument_function))
#else
#define SIM_NO_INSTRUMENT
#endif

extern "C" SIM_NO_INSTRUMENT void __cyg_profile_func_enter(void* function_address, void*) {
  sim::lpc_memory::enter_firmware_function(function_address);
}

extern "C" SIM_NO_INSTRUMENT void __cyg_profile_func_exit(void*, void*) { sim::lpc_memory::exit_firmware_function(); }

SIM_NO_INSTRUMENT void* operator new(std::size_t bytes) {
  void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
  if (pointer == nullptr) {
    throw std::bad_alloc();
  }
  sim::lpc_memory::record_host_main_allocation(pointer, bytes, false);
  return pointer;
}

SIM_NO_INSTRUMENT void* operator new[](std::size_t bytes) {
  void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
  if (pointer == nullptr) {
    throw std::bad_alloc();
  }
  sim::lpc_memory::record_host_main_allocation(pointer, bytes, true);
  return pointer;
}

SIM_NO_INSTRUMENT void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
  void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
  sim::lpc_memory::record_host_main_allocation(pointer, bytes, false);
  return pointer;
}

SIM_NO_INSTRUMENT void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
  void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
  sim::lpc_memory::record_host_main_allocation(pointer, bytes, true);
  return pointer;
}

SIM_NO_INSTRUMENT void operator delete(void* pointer) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

SIM_NO_INSTRUMENT void operator delete[](void* pointer) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

SIM_NO_INSTRUMENT void operator delete(void* pointer, std::size_t) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

SIM_NO_INSTRUMENT void operator delete[](void* pointer, std::size_t) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

SIM_NO_INSTRUMENT void operator delete(void* pointer, const std::nothrow_t&) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

SIM_NO_INSTRUMENT void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

#undef SIM_NO_INSTRUMENT
