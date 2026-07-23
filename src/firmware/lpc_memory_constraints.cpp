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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

#include "StreamOutput.h"
#include "sim/system_reset.hpp"

#ifndef STACK_SIZE
#define STACK_SIZE 4096
#endif

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
std::uint8_t* g_heap_break = g_main_sram;
const std::uint8_t* g_heap_limit =
    g_main_sram + (sim::lpc_memory::kMainSramBytes - static_cast<std::size_t>(STACK_SIZE));

thread_local const void* g_stack_sample_base = nullptr;

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
}

}  // namespace

SimMemoryPool simulator_ahb;

SimMemoryPool::SimMemoryPool() = default;

void SimMemoryPool::rebuild_capped_pool() {
  delete[] arena_;
  arena_ = nullptr;
  arena_bytes_ = capacity_;
  if (arena_bytes_ < kHeaderSize * 2) {
    arena_bytes_ = 0;
    return;
  }
  arena_ = new (std::nothrow) std::uint8_t[arena_bytes_];
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
  delete[] arena_;
  arena_ = nullptr;
  arena_bytes_ = 0;
  unlimited_ = false;
  capacity_ = g_ahb_capacity;
  ensure_configured();
}

std::size_t SimMemoryPool::capacity() const { return unlimited_ ? 0 : capacity_; }

bool SimMemoryPool::unlimited() const { return unlimited_; }

void* SimMemoryPool::alloc(std::size_t bytes) {
  std::lock_guard<std::mutex> lock(g_mutex);
  ensure_configured();
  if (bytes == 0) {
    bytes = 1;
  }
  bytes = align4(bytes);

  if (unlimited_) {
    return std::malloc(bytes);
  }
  if (arena_ == nullptr || arena_bytes_ == 0) {
    return nullptr;
  }

  const std::uint32_t need = static_cast<std::uint32_t>(bytes + kHeaderSize);
  std::uint8_t* cursor = arena_;
  const std::uint8_t* end = arena_ + arena_bytes_;
  while (cursor + kHeaderSize <= end) {
    auto* header = reinterpret_cast<PoolHeader*>(cursor);
    const std::uint32_t block = header_size(header->word);
    if (block < kHeaderSize || cursor + block > end) {
      return nullptr;
    }
    if (!header_used(header->word) && block >= need) {
      if (block >= need + kMinSplitTail) {
        auto* next = reinterpret_cast<PoolHeader*>(cursor + need);
        next->word = make_header(false, block - need);
        header->word = make_header(true, need);
      } else {
        header->word = make_header(true, block);
      }
      return cursor + kHeaderSize;
    }
    cursor += block;
  }
  return nullptr;
}

void SimMemoryPool::dealloc(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
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
    g_heap_break = g_main_sram;
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
  g_heap_break = g_main_sram;
}

bool lpc_heap_enabled() {
  apply_environment_defaults();
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_lpc_heap_enabled;
}

void* sbrk(int increment) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (increment == 0) {
    return g_heap_break;
  }
  if (increment < 0) {
    const auto dec = static_cast<std::size_t>(-increment);
    if (g_heap_break - g_main_sram < static_cast<std::ptrdiff_t>(dec)) {
      return reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
    }
    g_heap_break -= dec;
    return g_heap_break;
  }

  const auto inc = static_cast<std::size_t>(increment);
  if (g_heap_break + inc > g_heap_limit) {
    system_reset::request();
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
  }
  void* previous = g_heap_break;
  g_heap_break += inc;
  return previous;
}

}  // namespace sim::lpc_memory
