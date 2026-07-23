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

#ifndef SIMULATOR_SIM_LPC_MEMORY_CONSTRAINTS_HPP
#define SIMULATOR_SIM_LPC_MEMORY_CONSTRAINTS_HPP

#include <cstddef>
#include <cstdint>

class StreamOutput;

namespace sim::lpc_memory {

// Dynamic AHB MemoryPool capacity from the pinned LPC1768 map:
// __AHB_end (0x20084000) - __AHB_dyn_start (0x2007fb78) = 0x4488.
inline constexpr std::size_t kLpcAhbPoolBytes = 17544;
inline constexpr std::size_t kMainSramBytes = 32768;
inline constexpr std::size_t kLpcStackSize = 4096;
// 350 ConfigValue entries × 26 bytes — matches device CONFIG_CACHE_CAPACITY.
inline constexpr std::size_t kConfigCacheBytes = 9100;
// Typical leftover between end of .data/.bss and StackTop on current LPC maps.
inline constexpr std::size_t kTypicalPostStaticBytes = 25000;
inline constexpr std::size_t kSimulatedStaticBytes = kMainSramBytes - kTypicalPostStaticBytes;
// Headroom under the config cache when the window is reserved. Chosen so normal
// live-window fopen/small-alloc traffic still clears, while the extra flex
// always-active fopen + std::function traffic tips past cache_start.
inline constexpr std::size_t kConfigCacheBootHeadroomBytes = 1800;
// newlib FILE buffer size charged while the config cache is live.
inline constexpr std::size_t kNewlibFileBufferBytes = 1024;

// Host builds use 64-bit pointers, so AHB-resident objects (Block queue, etc.)
// are larger than on the MCU. Scale the default pool by pointer width so boot
// still mirrors LPC headroom; use --ahb-bytes 17544 for strict LPC capacity.
inline constexpr std::size_t kDefaultAhbPoolBytes =
    (sizeof(void*) > 4) ? (kLpcAhbPoolBytes * sizeof(void*) / 4) : kLpcAhbPoolBytes;

void apply_environment_defaults();

void set_ahb_capacity(std::size_t bytes);
std::size_t ahb_capacity();
bool ahb_unlimited();

void set_native_stack_limit(std::size_t bytes);
std::size_t native_stack_limit();
void begin_firmware_stack_sample();
void check_firmware_stack_sample();

void reset_for_reboot();
std::uint64_t firmware_reboot_count();
void note_firmware_reboot();

// LPC main-SRAM model: 32KiB arena, stack reserve 4096, config cache at
// StackLimit - 9100. While the config cache is live (ConfigCache lifetime),
// host `fopen` and `new` advance a shadow `_sbrk` watermark so Config's
// heap↔cache check matches device behavior. There is no special-case for
// flex compensation — any live-window heap pressure can tip the watermark.
void set_lpc_heap_enabled(bool enabled);
bool lpc_heap_enabled();

void note_config_cache_live(bool live);
bool config_cache_live();

void* config_cache_base(std::size_t bytes);
std::uintptr_t stack_limit_address();
std::uintptr_t heap_break_address();
std::uintptr_t maximum_heap_address();

void* sbrk(int increment);

// Advance the shadow watermark (newlib FILE buffer from fopen). No-op unless
// LPC heap is enabled and the config cache is currently live.
void account_host_allocation_raw(std::size_t bytes);

}  // namespace sim::lpc_memory

class SimMemoryPool {
 public:
  SimMemoryPool();

  void* alloc(std::size_t bytes);
  void dealloc(void* ptr);
  std::uint32_t free() const;
  void debug(StreamOutput* stream) const;
  void reset();

  std::size_t capacity() const;
  bool unlimited() const;

 private:
  void ensure_configured();
  void rebuild_capped_pool();

  bool configured_{false};
  bool unlimited_{false};
  std::size_t capacity_{sim::lpc_memory::kDefaultAhbPoolBytes};
  std::uint8_t* arena_{nullptr};
  std::size_t arena_bytes_{0};
};

extern SimMemoryPool simulator_ahb;

#endif
