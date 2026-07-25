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

#include "sim/lpc_memory_constraints.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>

#include "Block.h"
#include "Conveyor.h"
#include "SlowTicker.h"
#include "StepTicker.h"
#include "StreamOutput.h"
#include "compat/active_context.hpp"
#include "lpc_memory_layout.hpp"
#include "modules/communication/SerialConsole.h"
#include "modules/communication/SerialConsole2.h"
#include "sim/lpc_memory_accounting.hpp"
#include "sim/simulator_context.hpp"
#include "sim/system_reset.hpp"

#ifndef STACK_SIZE
#define STACK_SIZE 4096
#endif

extern unsigned int g_maximumHeapAddress;

namespace {

constexpr std::uint32_t kUsedBit = 0x80000000u;
constexpr std::size_t kHeaderSize = 4;
constexpr std::size_t kMinSplitTail = kHeaderSize + 4;
struct alignas(4) PoolHeader {
  std::uint32_t word;
};

std::mutex g_mutex;
std::size_t g_ahb_capacity = sim::lpc_memory::kDefaultAhbPoolBytes;
std::size_t g_native_stack_limit = 0;
std::uint64_t g_reboot_count = 0;
bool g_env_applied = false;
bool g_lpc_heap_enabled = false;
alignas(8) std::uint8_t g_main_sram[sim::lpc_memory::kMainSramBytes];

std::uint8_t* heap_region_start() {
  return g_main_sram + sim::lpc_memory::kSimulatedStaticBytes;
}

std::uint8_t* stack_limit_ptr() {
  return g_main_sram + (sim::lpc_memory::kMainSramBytes - static_cast<std::size_t>(STACK_SIZE));
}

std::uint8_t* maximum_heap_ptr() {
  // Match mbed_custom: StackLimit - 32 (MPU guard), allowing growth through the
  // config-cache window so Config's software check can observe a collision.
  return stack_limit_ptr() - 32;
}

std::uint8_t* config_cache_ptr(std::size_t bytes) {
  return stack_limit_ptr() - bytes;
}

std::uint8_t* g_heap_break = heap_region_start();

thread_local const void* g_stack_sample_base = nullptr;
bool g_config_cache_live = false;

std::uint32_t header_size(std::uint32_t word) { return word & ~kUsedBit; }
bool header_used(std::uint32_t word) { return (word & kUsedBit) != 0; }
std::uint32_t make_header(bool used, std::uint32_t size) {
  return (used ? kUsedBit : 0u) | (size & ~kUsedBit);
}

std::size_t align4(std::size_t bytes) { return (bytes + 3u) & ~std::size_t{3}; }

bool env_truthy(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return false;
  }
  const std::string text = value;
  return text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "YES";
}

void publish_maximum_heap_address_locked() {
  if (g_lpc_heap_enabled) {
    g_maximumHeapAddress = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(maximum_heap_ptr()));
  } else {
    g_maximumHeapAddress = 0;
  }
}

void reset_heap_break_locked() {
  g_heap_break = heap_region_start();
  g_config_cache_live = false;
  publish_maximum_heap_address_locked();
}

void advance_heap_break_locked(std::size_t bytes) {
  if (bytes == 0) {
    return;
  }
  // Allow growth through the config-cache window up to the MPU ceiling. Do not
  // request reset here — Config::config_cache_clear FATALS when `_sbrk(0)` has
  // crossed cache_start. Resetting on every host-pressure charge caused false
  // boot loops with always_active=false.
  std::uint8_t* limit = maximum_heap_ptr();
  if (g_heap_break >= limit) {
    return;
  }
  if (g_heap_break + bytes > limit) {
    g_heap_break = limit;
    return;
  }
  g_heap_break += bytes;
}

void apply_environment_defaults_locked() {
  if (g_env_applied) {
    return;
  }
  g_env_applied = true;

  if (env_truthy("CARVERA_SIM_AHB_UNLIMITED")) {
    g_ahb_capacity = 0;
  } else if (const char* bytes = std::getenv("CARVERA_SIM_AHB_BYTES"); bytes != nullptr && *bytes != '\0') {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(bytes, &end, 0);
    if (end != bytes) {
      g_ahb_capacity = static_cast<std::size_t>(parsed);
    }
  }

  if (const char* stack = std::getenv("CARVERA_SIM_STACK_LIMIT_BYTES"); stack != nullptr && *stack != '\0') {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(stack, &end, 0);
    if (end != stack) {
      g_native_stack_limit = static_cast<std::size_t>(parsed);
    }
  }

  if (env_truthy("CARVERA_SIM_LPC_HEAP")) {
    g_lpc_heap_enabled = true;
  }

  reset_heap_break_locked();
}

}  // namespace

SimMemoryPool simulator_ahb;

namespace {

struct AhbTypeLayout {
  std::size_t host_bytes;
  std::uint32_t target_bytes;
  const char* name;
};

std::tuple<std::size_t, std::string, bool> target_ahb_payload(std::size_t host_bytes) {
  if (host_bytes >= 2 * sizeof(Block) && host_bytes % sizeof(Block) == 0) {
    return {
        (host_bytes / sizeof(Block)) * sim::lpc_memory::generated::kBlockBytes,
        "Block[]",
        true,
    };
  }

  constexpr std::array layouts{
      AhbTypeLayout{sizeof(Conveyor), sim::lpc_memory::generated::kConveyorBytes, "Conveyor"},
      AhbTypeLayout{sizeof(SerialConsole), sim::lpc_memory::generated::kSerialConsoleBytes, "SerialConsole"},
      AhbTypeLayout{sizeof(SerialConsole2), sim::lpc_memory::generated::kSerialConsole2Bytes, "SerialConsole2"},
      AhbTypeLayout{sizeof(SlowTicker), sim::lpc_memory::generated::kSlowTickerBytes, "SlowTicker"},
      AhbTypeLayout{sizeof(StepTicker), sim::lpc_memory::generated::kStepTickerBytes, "StepTicker"},
  };
  const AhbTypeLayout* match = nullptr;
  for (const auto& layout : layouts) {
    if (layout.host_bytes != host_bytes) {
      continue;
    }
    if (match != nullptr &&
        (match->target_bytes != layout.target_bytes || std::string_view(match->name) != layout.name)) {
      return {host_bytes, "ABI-unresolved", false};
    }
    match = &layout;
  }
  if (match != nullptr) {
    return {match->target_bytes, match->name, true};
  }
  // Every remaining direct AHB.alloc() call in the pinned firmware is a byte
  // or float buffer, whose requested byte count is identical on the LPC.
  return {host_bytes, "raw buffer", true};
}

void record_ahb_allocation(void* pointer, std::size_t host_bytes) {
  if (pointer == nullptr) {
    return;
  }
  try {
    if (auto* context = sim::compat::try_active_context(); context != nullptr) {
      const auto [target_bytes, type_name, exact] = target_ahb_payload(host_bytes);
      context->memory_accounting().record_ahb(pointer, host_bytes, target_bytes, type_name, exact);
    }
  } catch (...) {
  }
}

}  // namespace

SimMemoryPool::SimMemoryPool() = default;

void SimMemoryPool::rebuild_capped_pool() {
  // Use malloc/free so AHB arena pages are not charged to the main-SRAM `_sbrk`
  // watermark (on device AHB is a separate bank).
  std::free(arena_);
  arena_ = nullptr;
  arena_bytes_ = capacity_;
  if (arena_bytes_ < kHeaderSize * 2) {
    arena_bytes_ = 0;
    return;
  }
  arena_ = static_cast<std::uint8_t*>(std::malloc(arena_bytes_));
  if (arena_ == nullptr) {
    arena_bytes_ = 0;
    return;
  }
  std::memset(arena_, 0, arena_bytes_);
  auto* head = reinterpret_cast<PoolHeader*>(arena_);
  head->word = make_header(false, static_cast<std::uint32_t>(arena_bytes_));
}

void SimMemoryPool::ensure_configured() {
  apply_environment_defaults_locked();
  if (configured_) {
    return;
  }
  unlimited_ = (g_ahb_capacity == 0);
  capacity_ = g_ahb_capacity;
  configured_ = true;
  if (!unlimited_) {
    rebuild_capped_pool();
  }
}

void SimMemoryPool::reset() {
  std::lock_guard<std::mutex> lock(g_mutex);
  apply_environment_defaults_locked();
  configured_ = false;
  std::free(arena_);
  arena_ = nullptr;
  arena_bytes_ = 0;
  unlimited_ = false;
  capacity_ = g_ahb_capacity;
  ensure_configured();
}

std::size_t SimMemoryPool::capacity() const { return unlimited_ ? 0 : capacity_; }

bool SimMemoryPool::unlimited() const { return unlimited_; }

void* SimMemoryPool::alloc(std::size_t bytes) {
  void* pointer = nullptr;
  std::size_t requested = bytes;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    ensure_configured();
    if (bytes == 0) {
      bytes = 1;
    }
    bytes = align4(bytes);
    requested = bytes;

    if (unlimited_) {
      pointer = std::malloc(bytes);
    } else if (arena_ != nullptr && arena_bytes_ != 0) {
      const std::uint32_t need = static_cast<std::uint32_t>(bytes + kHeaderSize);
      std::uint8_t* cursor = arena_;
      const std::uint8_t* end = arena_ + arena_bytes_;
      while (cursor + kHeaderSize <= end) {
        auto* header = reinterpret_cast<PoolHeader*>(cursor);
        const std::uint32_t block = header_size(header->word);
        if (block < kHeaderSize || cursor + block > end) {
          break;
        }
        if (!header_used(header->word) && block >= need) {
          if (block >= need + kMinSplitTail) {
            auto* next = reinterpret_cast<PoolHeader*>(cursor + need);
            next->word = make_header(false, block - need);
            header->word = make_header(true, need);
          } else {
            header->word = make_header(true, block);
          }
          pointer = cursor + kHeaderSize;
          break;
        }
        cursor += block;
      }
    }
  }
  record_ahb_allocation(pointer, requested);
  return pointer;
}

void SimMemoryPool::dealloc(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  sim::lpc_memory::release_host_allocation(ptr);
  std::lock_guard<std::mutex> lock(g_mutex);
  ensure_configured();
  if (unlimited_) {
    std::free(ptr);
    return;
  }
  if (arena_ == nullptr) {
    return;
  }

  auto* header = reinterpret_cast<PoolHeader*>(static_cast<std::uint8_t*>(ptr) - kHeaderSize);
  if (reinterpret_cast<std::uint8_t*>(header) < arena_ ||
      reinterpret_cast<std::uint8_t*>(header) >= arena_ + arena_bytes_) {
    return;
  }
  if (!header_used(header->word)) {
    return;
  }
  header->word = make_header(false, header_size(header->word));

  std::uint8_t* cursor = arena_;
  const std::uint8_t* end = arena_ + arena_bytes_;
  while (cursor + kHeaderSize <= end) {
    auto* current = reinterpret_cast<PoolHeader*>(cursor);
    const std::uint32_t current_size = header_size(current->word);
    if (current_size < kHeaderSize || cursor + current_size > end) {
      break;
    }
    if (!header_used(current->word)) {
      std::uint8_t* look = cursor + current_size;
      std::uint32_t merged = current_size;
      while (look + kHeaderSize <= end) {
        auto* next = reinterpret_cast<PoolHeader*>(look);
        const std::uint32_t next_size = header_size(next->word);
        if (next_size < kHeaderSize || look + next_size > end || header_used(next->word)) {
          break;
        }
        merged += next_size;
        look += next_size;
      }
      current->word = make_header(false, merged);
    }
    cursor += header_size(reinterpret_cast<PoolHeader*>(cursor)->word);
  }
}

std::uint32_t SimMemoryPool::free() const {
  std::lock_guard<std::mutex> lock(g_mutex);
  const_cast<SimMemoryPool*>(this)->ensure_configured();
  if (unlimited_) {
    return 0xffffffffu;
  }
  if (arena_ == nullptr) {
    return 0;
  }
  std::uint32_t total = 0;
  std::uint8_t* cursor = arena_;
  const std::uint8_t* end = arena_ + arena_bytes_;
  while (cursor + kHeaderSize <= end) {
    auto* header = reinterpret_cast<PoolHeader*>(cursor);
    const std::uint32_t block = header_size(header->word);
    if (block < kHeaderSize || cursor + block > end) {
      break;
    }
    if (!header_used(header->word) && block > kHeaderSize) {
      total += block - static_cast<std::uint32_t>(kHeaderSize);
    }
    cursor += block;
  }
  return total;
}

void SimMemoryPool::debug(StreamOutput* stream) const {
  if (stream == nullptr) {
    return;
  }
  const std::uint32_t free_bytes = free();
  std::lock_guard<std::mutex> lock(g_mutex);
  const_cast<SimMemoryPool*>(this)->ensure_configured();
  if (unlimited_) {
    stream->printf("SimMemoryPool: unlimited (malloc)\n");
    return;
  }
  stream->printf("SimMemoryPool at %p: Size=%zu, TotalFree=%lu\n", static_cast<void*>(arena_), arena_bytes_,
                 static_cast<unsigned long>(free_bytes));
}

namespace sim::lpc_memory {

void apply_environment_defaults() {
  std::lock_guard<std::mutex> lock(g_mutex);
  apply_environment_defaults_locked();
}

void set_ahb_capacity(std::size_t bytes) {
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_env_applied = true;
    g_ahb_capacity = bytes;
  }
  simulator_ahb.reset();
}

std::size_t ahb_capacity() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_ahb_capacity;
}

bool ahb_unlimited() { return ahb_capacity() == 0; }

void set_native_stack_limit(std::size_t bytes) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_env_applied = true;
  g_native_stack_limit = bytes;
  g_stack_sample_base = nullptr;
}

std::size_t native_stack_limit() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_native_stack_limit;
}

void begin_firmware_stack_sample() {
  if (native_stack_limit() == 0) {
    return;
  }
  g_stack_sample_base = __builtin_frame_address(0);
}

void check_firmware_stack_sample() {
  const std::size_t limit = native_stack_limit();
  if (limit == 0 || g_stack_sample_base == nullptr) {
    return;
  }
  const auto* sp = static_cast<const std::uint8_t*>(__builtin_frame_address(0));
  const auto* base = static_cast<const std::uint8_t*>(g_stack_sample_base);
  if (sp > base) {
    return;
  }
  const std::size_t used = static_cast<std::size_t>(base - sp);
  if (used > limit) {
    system_reset::request();
  }
}

void reset_for_reboot() {
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    reset_heap_break_locked();
    g_stack_sample_base = nullptr;
  }
  simulator_ahb.reset();
}

void note_firmware_reboot() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_reboot_count;
}

std::uint64_t firmware_reboot_count() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_reboot_count;
}

void set_lpc_heap_enabled(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_env_applied = true;
  g_lpc_heap_enabled = enabled;
  reset_heap_break_locked();
}

bool lpc_heap_enabled() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_lpc_heap_enabled;
}

void note_config_cache_live(bool live) {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  g_config_cache_live = live;
}

bool config_cache_live() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_config_cache_live;
}

void* config_cache_base(std::size_t bytes) {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  if (bytes == 0) {
    bytes = kConfigCacheBytes;
  }
  if (bytes > kConfigCacheBytes) {
    bytes = kConfigCacheBytes;
  }
  std::uint8_t* cache = config_cache_ptr(bytes);
  // When the cache window is first reserved, raise the heap watermark to a
  // realistic post-BSS / early-boot level just below it. Host `new` sizes are
  // not MCU-accurate; fopen + small live-window allocs then provide the natural
  // tip-over (e.g. flex always-active) without special-casing that path.
  if (g_heap_break < cache - kConfigCacheBootHeadroomBytes) {
    std::uint8_t* target = cache - kConfigCacheBootHeadroomBytes;
    if (target > heap_region_start()) {
      g_heap_break = target;
    }
  }
  return cache;
}

std::uintptr_t stack_limit_address() {
  apply_environment_defaults();
  return reinterpret_cast<std::uintptr_t>(stack_limit_ptr());
}

std::uintptr_t heap_break_address() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return reinterpret_cast<std::uintptr_t>(g_heap_break);
}

std::uintptr_t maximum_heap_address() {
  apply_environment_defaults();
  return reinterpret_cast<std::uintptr_t>(maximum_heap_ptr());
}

void* sbrk(int increment) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (increment == 0) {
    return g_heap_break;
  }
  if (increment < 0) {
    const auto dec = static_cast<std::size_t>(-increment);
    if (g_heap_break - heap_region_start() < static_cast<std::ptrdiff_t>(dec)) {
      return reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
    }
    g_heap_break -= dec;
    return g_heap_break;
  }

  const auto inc = static_cast<std::size_t>(increment);
  std::uint8_t* previous = g_heap_break;
  if (g_heap_break + inc > maximum_heap_ptr()) {
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
  }
  g_heap_break += inc;
  return previous;
}

void account_host_allocation_raw(std::size_t bytes) {
  if (bytes == 0) {
    return;
  }
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_lpc_heap_enabled || !g_config_cache_live) {
    return;
  }
  advance_heap_break_locked(bytes);
}

}  // namespace sim::lpc_memory

