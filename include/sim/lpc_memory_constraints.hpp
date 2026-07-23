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
// Host builds use 64-bit pointers, so AHB-resident objects (Block queue, etc.)
// are larger than on the MCU. Scale the default pool by pointer width so boot
// still mirrors LPC headroom; use --ahb-bytes 17544 for strict LPC capacity.
inline constexpr std::size_t kDefaultAhbPoolBytes =
    (sizeof(void*) > 4) ? (kLpcAhbPoolBytes * sizeof(void*) / 4) : kLpcAhbPoolBytes;

// Apply CARVERA_SIM_AHB_BYTES / CARVERA_SIM_AHB_UNLIMITED /
// CARVERA_SIM_STACK_LIMIT_BYTES once at process start.
void apply_environment_defaults();

// 0 = unlimited malloc shim (legacy host behavior).
void set_ahb_capacity(std::size_t bytes);
std::size_t ahb_capacity();
bool ahb_unlimited();

// Optional native stack watermark for the firmware pump thread.
// 0 disables the check. A literal LPC STACK_SIZE (4096) will false-trigger on
// host C++ frames; use a larger budget (e.g. 256KiB) when hunting deep recursion.
void set_native_stack_limit(std::size_t bytes);
std::size_t native_stack_limit();
void begin_firmware_stack_sample();
void check_firmware_stack_sample();

// Rebuild the AHB pool and simulated main-SRAM heap (LPC reboot semantics).
void reset_for_reboot();
std::uint64_t firmware_reboot_count();
void note_firmware_reboot();

// Optional LPC main-SRAM heap model for `_sbrk`. Off by default because the
// host process still needs a normal C++ heap for Config and STL.
void set_lpc_heap_enabled(bool enabled);
bool lpc_heap_enabled();

// LPC-like `_sbrk` against a 32KiB main SRAM model with STACK_SIZE reserved.
// Only used when `lpc_heap_enabled()` is true.
void* sbrk(int increment);

}  // namespace sim::lpc_memory

// Bounded AHB pool used by firmware via `#define AHB simulator_ahb`.
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
