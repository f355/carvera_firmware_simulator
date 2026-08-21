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

#include "sim/lpc_memory_accounting.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include "StreamOutput.h"
#include "compat/active_context.hpp"
#include "lpc_memory_layout.hpp"
#include "sim/simulator_context.hpp"

namespace {

constexpr std::uint32_t kHeapHeaderBytes = 8;
constexpr std::uint32_t kHeapAlignment = 8;
constexpr std::uint32_t kHeapMinimumBlockBytes = 2 * kHeapHeaderBytes;
constexpr std::uint32_t kHeapRegionSentinelBytes = 8;

struct FirmwareCallStack {
  static constexpr std::size_t kMaximumTrackedDepth = 128;

  std::array<void*, kMaximumTrackedDepth> functions{};
  std::size_t depth{};
  bool accounting_suppressed{};
};

thread_local FirmwareCallStack firmware_calls;

class AccountingSuppression {
 public:
  AccountingSuppression() : previous_(firmware_calls.accounting_suppressed) {
    firmware_calls.accounting_suppressed = true;
  }
  ~AccountingSuppression() { firmware_calls.accounting_suppressed = previous_; }

 private:
  bool previous_;
};

std::uint32_t checked_size(std::size_t bytes) {
  if (bytes > std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("LPC allocation is larger than a 32-bit address space");
  }
  return static_cast<std::uint32_t>(bytes);
}

std::uint32_t aligned_span(std::size_t payload_bytes, std::uint32_t header_bytes, std::uint32_t alignment) {
  const auto payload = checked_size(payload_bytes);
  if (payload > std::numeric_limits<std::uint32_t>::max() - header_bytes - (alignment - 1)) {
    throw std::overflow_error("LPC allocation size overflow");
  }
  return (payload + header_bytes + alignment - 1) & ~(alignment - 1);
}

template <typename Chunk>
void rebuild_allocation_index(const std::vector<Chunk>& chunks,
                              std::unordered_map<sim::lpc_memory::AllocationId, std::size_t>& index) {
  index.clear();
  for (std::size_t i = 0; i < chunks.size(); ++i) {
    if (chunks[i].used) {
      index.emplace(chunks[i].allocation_id, i);
    }
  }
}

}  // namespace

namespace sim::lpc_memory {

MainSramLayout firmware_main_sram_layout() {
  return {
      .ram_start = generated::kRamStart,
      .ram_end = generated::kRamEnd,
      .static_end = generated::kStaticEnd,
      .stack_top = generated::kStackTop,
      .stack_limit = generated::kStackLimit,
      .heap_start = generated::kMainHeapStart,
      .heap_end = generated::kMainHeapEnd,
  };
}

AhbLayout firmware_ahb_layout() {
  return {
      .region_start = generated::kAhbRegionStart,
      .region_end = generated::kAhbRegionEnd,
      .static_end = generated::kAhbStaticEnd,
      .heap_start = generated::kAhbHeapStart,
      .heap_end = generated::kAhbHeapEnd,
  };
}

MemoryAccounting::MemoryAccounting() : heap_(firmware_main_sram_layout(), firmware_ahb_layout()) {}

void MemoryAccounting::record_heap(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                                   std::string type_name, bool target_size_exact) {
  record(pointer, host_payload_bytes, target_payload_bytes, std::move(type_name), target_size_exact);
}

void MemoryAccounting::record(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                              std::string type_name, bool target_size_exact) {
  if (pointer == nullptr) {
    return;
  }

  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  if (allocations_.contains(pointer)) {
    throw std::invalid_argument("host pointer is already tracked by LPC memory accounting");
  }

  const AllocationId id = next_id_++;
  bool modeled = false;
  MemoryRegion region = MemoryRegion::UnifiedHeap;
  if (target_size_exact) {
    if (const auto allocation_region = heap_.allocate(id, target_payload_bytes); allocation_region.has_value()) {
      region = *allocation_region;
      modeled = true;
    }
  }
  const auto group_index =
      find_or_create_group(region, std::move(type_name), host_payload_bytes, target_payload_bytes, target_size_exact);
  allocations_.emplace(pointer, Allocation{
                                    .region = region,
                                    .id = id,
                                    .group_index = group_index,
                                    .modeled = modeled,
                                });

  charge_group(group_index, target_payload_bytes);
}

std::size_t MemoryAccounting::find_or_create_group(MemoryRegion region, std::string type_name,
                                                   std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                                                   bool target_size_exact) {
  for (std::size_t index = 0; index < groups_.size(); ++index) {
    const auto& group = groups_[index];
    if (group.region == region && group.type_name == type_name && group.host_payload_bytes == host_payload_bytes &&
        group.target_payload_bytes == target_payload_bytes && group.target_size_exact == target_size_exact) {
      return index;
    }
  }
  groups_.push_back(AllocationGroupSnapshot{
      .region = region,
      .type_name = std::move(type_name),
      .host_payload_bytes = checked_size(host_payload_bytes),
      .target_payload_bytes = checked_size(target_payload_bytes),
      .target_size_exact = target_size_exact,
  });
  return groups_.size() - 1;
}

void MemoryAccounting::charge_group(std::size_t group_index, std::size_t target_payload_bytes) {
  auto& group = groups_[group_index];
  ++group.live_count;
  ++group.total_count;
  group.peak_live_count = std::max(group.peak_live_count, group.live_count);
  group.live_target_bytes += target_payload_bytes;
  group.peak_target_bytes = std::max(group.peak_target_bytes, group.live_target_bytes);
}

std::optional<MemoryRegion> MemoryAccounting::deallocate(void* pointer) {
  if (pointer == nullptr) {
    return std::nullopt;
  }

  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  const auto found = allocations_.find(pointer);
  if (found == allocations_.end()) {
    return std::nullopt;
  }

  const auto allocation = found->second;
  if (allocation.modeled) {
    heap_.deallocate(allocation.id);
  }
  auto& group = groups_[allocation.group_index];
  --group.live_count;
  group.live_target_bytes -= group.target_payload_bytes;
  allocations_.erase(found);
  return allocation.region;
}

void MemoryAccounting::release_config_cache() {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  for (auto allocation = allocations_.begin(); allocation != allocations_.end();) {
    auto& group = groups_[allocation->second.group_index];
    if (group.type_name != "ConfigCache::Chunk") {
      ++allocation;
      continue;
    }
    if (allocation->second.modeled) {
      heap_.deallocate(allocation->second.id);
    }
    --group.live_count;
    group.live_target_bytes -= group.target_payload_bytes;
    allocation = allocations_.erase(allocation);
  }
}

void MemoryAccounting::reset() {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  heap_.reset();
  allocations_.clear();
  groups_.clear();
  next_id_ = 1;
}

MemoryAccountingSnapshot MemoryAccounting::snapshot() const {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  const auto heap = heap_.snapshot();
  return {
      .heap = heap.heap,
      .main = heap.main,
      .ahb = heap.ahb,
      .allocation_groups = groups_,
  };
}

void enter_firmware_function(void* function_address) noexcept {
  if (firmware_calls.depth < firmware_calls.functions.size()) {
    firmware_calls.functions[firmware_calls.depth] = function_address;
  }
  ++firmware_calls.depth;
}

void exit_firmware_function() noexcept {
  if (firmware_calls.depth > 0) {
    --firmware_calls.depth;
  }
}

bool firmware_allocation_active() noexcept { return firmware_calls.depth > 0 && !firmware_calls.accounting_suppressed; }

void record_host_heap_allocation(void* pointer, std::size_t host_payload_bytes, bool array_allocation) noexcept {
  if (pointer == nullptr || !firmware_allocation_active()) {
    return;
  }

  AccountingSuppression suppression;
  try {
    auto* context = compat::try_active_context();
    if (context == nullptr) {
      return;
    }
    std::string allocation_implementation;
    std::string origin;
    const auto tracked_depth = std::min(firmware_calls.depth, firmware_calls.functions.size());
    for (std::size_t index = tracked_depth; index > 0; --index) {
      auto function = describe_firmware_function(firmware_calls.functions[index - 1]);
      if (allocation_implementation.empty()) {
        allocation_implementation = function;
      }
      if (!is_allocation_runtime_function(function)) {
        origin = std::move(function);
        break;
      }
    }
    if (origin.empty()) {
      origin = allocation_implementation;
    }
    auto resolved =
        resolve_generic_heap_allocation(host_payload_bytes, array_allocation, origin, allocation_implementation);
    context->memory_accounting().record_heap(pointer, host_payload_bytes, resolved.target_payload_bytes,
                                             std::move(resolved.type_name), resolved.target_size_exact);
  } catch (...) {
  }
}

bool release_host_allocation(void* pointer) noexcept {
  if (pointer == nullptr || firmware_calls.accounting_suppressed) {
    return false;
  }

  AccountingSuppression suppression;
  try {
    if (auto* context = compat::try_active_context(); context != nullptr) {
      return context->memory_accounting().deallocate(pointer).has_value();
    }
  } catch (...) {
  }
  return false;
}

char* tracked_strdup(const char* source) noexcept {
  if (source == nullptr) {
    return nullptr;
  }
  const auto bytes = std::strlen(source) + 1;
  auto* copy = static_cast<char*>(std::malloc(bytes));
  if (copy == nullptr) {
    return nullptr;
  }
  std::memcpy(copy, source, bytes);

  if (!firmware_allocation_active()) {
    return copy;
  }
  AccountingSuppression suppression;
  try {
    if (auto* context = compat::try_active_context(); context != nullptr) {
      context->memory_accounting().record_heap(copy, bytes, bytes, "strdup char[]", true);
    }
  } catch (...) {
  }
  return copy;
}

void tracked_free(void* pointer) noexcept {
  release_host_allocation(pointer);
  std::free(pointer);
}

void print_memory_report(StreamOutput* stream, bool verbose) {
  if (stream == nullptr) {
    return;
  }
  AccountingSuppression suppression;
  auto* context = compat::try_active_context();
  if (context == nullptr) {
    stream->printf("LPC memory accounting unavailable\n");
    return;
  }

  auto report = context->memory_accounting().snapshot();
  const auto& heap = report.heap;
  stream->printf("Heap free: %lu bytes, minimum ever free: %lu bytes\r\n",
                 static_cast<unsigned long>(heap.total_free_bytes),
                 static_cast<unsigned long>(heap.minimum_ever_free_bytes));
  stream->printf("Largest contiguous free area: %lu bytes, free areas: %lu\r\n",
                 static_cast<unsigned long>(heap.largest_free_block_bytes),
                 static_cast<unsigned long>(heap.free_area_count));

  const auto& main = report.main;
  stream->printf("LPC1768 Main SRAM: capacity=%lu static=%lu stack=%lu\n",
                 static_cast<unsigned long>(main.capacity_bytes), static_cast<unsigned long>(main.static_bytes),
                 static_cast<unsigned long>(main.stack_reserved_bytes));
  stream->printf("  Heap region: used=%lu live=%lu peak=%lu overhead=%lu free=%lu largest-free=%lu\n",
      static_cast<unsigned long>(main.heap_committed_bytes), static_cast<unsigned long>(main.live_payload_bytes),
      static_cast<unsigned long>(main.peak_live_payload_bytes),
      static_cast<unsigned long>(main.allocator_overhead_bytes), static_cast<unsigned long>(main.fragmented_free_bytes),
      static_cast<unsigned long>(main.largest_free_block_bytes));

  const auto& ahb = report.ahb;
  stream->printf("LPC1768 AHB SRAM: capacity=%lu static=%lu heap=%lu\n",
                 static_cast<unsigned long>(ahb.capacity_bytes), static_cast<unsigned long>(ahb.static_bytes),
                 static_cast<unsigned long>(ahb.dynamic_capacity_bytes));
  const auto ahb_used = ahb.live_payload_bytes + ahb.allocator_overhead_bytes;
  stream->printf("  Heap region: used=%lu live=%lu peak=%lu overhead=%lu free=%lu largest-free=%lu\n",
      static_cast<unsigned long>(ahb_used),
      static_cast<unsigned long>(ahb.live_payload_bytes), static_cast<unsigned long>(ahb.peak_live_payload_bytes),
      static_cast<unsigned long>(ahb.allocator_overhead_bytes), static_cast<unsigned long>(ahb.total_free_bytes),
      static_cast<unsigned long>(ahb.largest_free_block_bytes));
  stream->printf("Unified heap allocation failures: %lu (%llu requested bytes)\n",
                 static_cast<unsigned long>(heap.failed_allocation_count),
                 static_cast<unsigned long long>(heap.failed_allocation_bytes));

  std::uint64_t unresolved_main_live = 0;
  std::uint64_t unresolved_main_peak = 0;
  std::uint64_t unresolved_ahb_live = 0;
  std::uint64_t unresolved_ahb_peak = 0;
  std::uint64_t unresolved_heap_live = 0;
  std::uint64_t unresolved_heap_peak = 0;
  for (const auto& group : report.allocation_groups) {
    if (group.target_size_exact) {
      continue;
    }
    const auto live = static_cast<std::uint64_t>(group.host_payload_bytes) * group.live_count;
    const auto peak = static_cast<std::uint64_t>(group.host_payload_bytes) * group.peak_live_count;
    if (group.region == MemoryRegion::MainSram) {
      unresolved_main_live += live;
      unresolved_main_peak += peak;
    } else if (group.region == MemoryRegion::AhbSram) {
      unresolved_ahb_live += live;
      unresolved_ahb_peak += peak;
    } else {
      unresolved_heap_live += live;
      unresolved_heap_peak += peak;
    }
  }
  stream->printf(
      "Unresolved ABI allocations (not charged to LPC totals): main live=%llu peak=%llu; "
      "AHB live=%llu peak=%llu; unplaced live=%llu peak=%llu host-request bytes\n",
      static_cast<unsigned long long>(unresolved_main_live), static_cast<unsigned long long>(unresolved_main_peak),
      static_cast<unsigned long long>(unresolved_ahb_live), static_cast<unsigned long long>(unresolved_ahb_peak),
      static_cast<unsigned long long>(unresolved_heap_live), static_cast<unsigned long long>(unresolved_heap_peak));

  if (!verbose) {
    return;
  }
  stream->printf("Smallest free area: %lu bytes, allocations: %lu, frees: %lu\r\n",
                 static_cast<unsigned long>(heap.smallest_free_block_bytes),
                 static_cast<unsigned long>(heap.successful_allocation_count),
                 static_cast<unsigned long>(heap.successful_free_count));
  std::ranges::sort(report.allocation_groups, [](const auto& lhs, const auto& rhs) {
    if (lhs.region != rhs.region) {
      return lhs.region < rhs.region;
    }
    return lhs.peak_target_bytes > rhs.peak_target_bytes;
  });
  stream->printf("Allocation groups (host request -> LPC charge):\n");
  for (const auto& group : report.allocation_groups) {
    const char* region = group.region == MemoryRegion::MainSram
                             ? "main"
                             : (group.region == MemoryRegion::AhbSram ? "AHB" : "unplaced");
    const char* type = group.type_name.empty() ? "unlabelled" : group.type_name.c_str();
    stream->printf("  %s %-20s %lu -> %lu bytes, live=%lu peak=%lu total=%lu%s\n", region, type,
                   static_cast<unsigned long>(group.host_payload_bytes),
                   static_cast<unsigned long>(group.target_payload_bytes), static_cast<unsigned long>(group.live_count),
                   static_cast<unsigned long>(group.peak_live_count), static_cast<unsigned long>(group.total_count),
                   group.target_size_exact ? "" : " (host-size estimate)");
  }
}

UnifiedHeapModel::UnifiedHeapModel(MainSramLayout main_layout, AhbLayout ahb_layout)
    : main_layout_(main_layout), ahb_layout_(ahb_layout) {
  if (!(main_layout_.ram_start <= main_layout_.static_end && main_layout_.static_end == main_layout_.heap_start &&
        main_layout_.heap_start < main_layout_.heap_end && main_layout_.heap_end < main_layout_.stack_limit &&
        main_layout_.stack_limit <= main_layout_.stack_top && main_layout_.stack_top <= main_layout_.ram_end)) {
    throw std::invalid_argument("invalid LPC main SRAM heap layout");
  }
  if (!(ahb_layout_.region_start <= ahb_layout_.static_end && ahb_layout_.static_end == ahb_layout_.heap_start &&
        ahb_layout_.heap_start < ahb_layout_.heap_end && ahb_layout_.heap_end <= ahb_layout_.region_end)) {
    throw std::invalid_argument("invalid LPC AHB SRAM heap layout");
  }
  if ((main_layout_.heap_start & (kHeapAlignment - 1)) != 0 ||
      (ahb_layout_.heap_start & (kHeapAlignment - 1)) != 0) {
    throw std::invalid_argument("LPC heap_5 regions must be eight-byte aligned");
  }
  reset();
}

std::optional<MemoryRegion> UnifiedHeapModel::allocate(AllocationId id, std::size_t target_payload_bytes) {
  if (allocation_chunks_.contains(id)) {
    throw std::invalid_argument("duplicate LPC unified-heap allocation id");
  }

  const auto payload = checked_size(target_payload_bytes);
  const auto required = aligned_span(target_payload_bytes, kHeapHeaderBytes, kHeapAlignment);
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    if (chunks_[index].used || chunks_[index].span_bytes < required) {
      continue;
    }

    const auto remaining = chunks_[index].span_bytes - required;
    if (remaining > kHeapMinimumBlockBytes) {
      const Chunk tail{
          .region = chunks_[index].region,
          .address = chunks_[index].address + required,
          .span_bytes = remaining,
      };
      chunks_[index].span_bytes = required;
      chunks_.insert(chunks_.begin() + static_cast<std::ptrdiff_t>(index + 1), tail);
    }
    auto& chunk = chunks_[index];
    chunk.used = true;
    chunk.payload_bytes = payload;
    chunk.allocation_id = id;
    live_payload_bytes_ += payload;
    peak_live_payload_bytes_ = std::max(peak_live_payload_bytes_, live_payload_bytes_);
    auto& region_live = chunk.region == MemoryRegion::MainSram ? main_live_payload_bytes_ : ahb_live_payload_bytes_;
    auto& region_peak =
        chunk.region == MemoryRegion::MainSram ? main_peak_live_payload_bytes_ : ahb_peak_live_payload_bytes_;
    region_live += payload;
    region_peak = std::max(region_peak, region_live);
    ++successful_allocation_count_;
    rebuild_allocation_index(chunks_, allocation_chunks_);

    std::uint32_t total_free = 0;
    for (const auto& area : chunks_) {
      if (!area.used) {
        total_free += area.span_bytes;
      }
    }
    minimum_ever_free_bytes_ = std::min(minimum_ever_free_bytes_, total_free);
    return chunk.region;
  }

  ++failed_allocation_count_;
  failed_allocation_bytes_ += payload;
  return std::nullopt;
}

std::optional<MemoryRegion> UnifiedHeapModel::deallocate(AllocationId id) {
  const auto found = allocation_chunks_.find(id);
  if (found == allocation_chunks_.end()) {
    return std::nullopt;
  }
  auto& chunk = chunks_[found->second];
  const auto region = chunk.region;
  live_payload_bytes_ -= chunk.payload_bytes;
  auto& region_live = region == MemoryRegion::MainSram ? main_live_payload_bytes_ : ahb_live_payload_bytes_;
  region_live -= chunk.payload_bytes;
  chunk.payload_bytes = 0;
  chunk.allocation_id = 0;
  chunk.used = false;
  ++successful_free_count_;
  coalesce_free_chunks();
  rebuild_allocation_index(chunks_, allocation_chunks_);
  return region;
}

void UnifiedHeapModel::reset() {
  chunks_.clear();
  allocation_chunks_.clear();
  live_payload_bytes_ = 0;
  peak_live_payload_bytes_ = 0;
  main_live_payload_bytes_ = 0;
  main_peak_live_payload_bytes_ = 0;
  ahb_live_payload_bytes_ = 0;
  ahb_peak_live_payload_bytes_ = 0;
  failed_allocation_count_ = 0;
  failed_allocation_bytes_ = 0;
  successful_allocation_count_ = 0;
  successful_free_count_ = 0;

  const auto add_region = [this](MemoryRegion region, std::uint32_t start, std::uint32_t end) {
    const auto raw_capacity = end - start;
    if (raw_capacity <= kHeapRegionSentinelBytes) {
      throw std::invalid_argument("LPC heap_5 region is too small for its sentinel");
    }
    chunks_.push_back(Chunk{
        .region = region,
        .address = start,
        .span_bytes = raw_capacity - kHeapRegionSentinelBytes,
    });
  };
  add_region(MemoryRegion::MainSram, main_layout_.heap_start, main_layout_.heap_end);
  add_region(MemoryRegion::AhbSram, ahb_layout_.heap_start, ahb_layout_.heap_end);
  minimum_ever_free_bytes_ = 0;
  for (const auto& chunk : chunks_) {
    minimum_ever_free_bytes_ += chunk.span_bytes;
  }
}

UnifiedHeapModelSnapshot UnifiedHeapModel::snapshot() const {
  UnifiedHeapModelSnapshot result{
      .heap = {
          .live_payload_bytes = live_payload_bytes_,
          .peak_live_payload_bytes = peak_live_payload_bytes_,
          .minimum_ever_free_bytes = minimum_ever_free_bytes_,
          .smallest_free_block_bytes = std::numeric_limits<std::uint32_t>::max(),
          .successful_allocation_count = successful_allocation_count_,
          .successful_free_count = successful_free_count_,
          .failed_allocation_count = failed_allocation_count_,
          .failed_allocation_bytes = failed_allocation_bytes_,
      },
      .main = {
          .capacity_bytes = main_layout_.ram_end - main_layout_.ram_start,
          .static_bytes = main_layout_.static_end - main_layout_.ram_start,
          .stack_reserved_bytes = main_layout_.stack_top - main_layout_.stack_limit,
          .heap_break = main_layout_.heap_start,
          .active_heap_limit = main_layout_.heap_end,
          .live_payload_bytes = main_live_payload_bytes_,
          .peak_live_payload_bytes = main_peak_live_payload_bytes_,
          .minimum_margin_bytes = minimum_ever_free_bytes_,
          .failed_allocation_count = failed_allocation_count_,
          .failed_allocation_bytes = failed_allocation_bytes_,
      },
      .ahb = {
          .capacity_bytes = ahb_layout_.region_end - ahb_layout_.region_start,
          .static_bytes = ahb_layout_.static_end - ahb_layout_.region_start,
          .dynamic_capacity_bytes = ahb_layout_.heap_end - ahb_layout_.heap_start - kHeapRegionSentinelBytes,
          .live_payload_bytes = ahb_live_payload_bytes_,
          .peak_live_payload_bytes = ahb_peak_live_payload_bytes_,
      },
  };

  for (const auto& chunk : chunks_) {
    result.heap.capacity_bytes += chunk.span_bytes;
    if (chunk.used) {
      const auto overhead = chunk.span_bytes - chunk.payload_bytes;
      result.heap.allocator_overhead_bytes += overhead;
      if (chunk.region == MemoryRegion::MainSram) {
        result.main.heap_committed_bytes += chunk.span_bytes;
        result.main.allocator_overhead_bytes += overhead;
      } else {
        result.ahb.allocator_overhead_bytes += overhead;
      }
      continue;
    }

    result.heap.total_free_bytes += chunk.span_bytes;
    result.heap.largest_free_block_bytes = std::max(result.heap.largest_free_block_bytes, chunk.span_bytes);
    result.heap.smallest_free_block_bytes = std::min(result.heap.smallest_free_block_bytes, chunk.span_bytes);
    ++result.heap.free_area_count;
    if (chunk.region == MemoryRegion::MainSram) {
      result.main.fragmented_free_bytes += chunk.span_bytes;
      result.main.largest_free_block_bytes = std::max(result.main.largest_free_block_bytes, chunk.span_bytes);
    } else {
      result.ahb.total_free_bytes += chunk.span_bytes;
      result.ahb.largest_free_block_bytes = std::max(result.ahb.largest_free_block_bytes, chunk.span_bytes);
    }
  }
  if (result.heap.free_area_count == 0) {
    result.heap.smallest_free_block_bytes = 0;
  }
  return result;
}

void UnifiedHeapModel::coalesce_free_chunks() {
  for (std::size_t index = 0; index + 1 < chunks_.size();) {
    auto& current = chunks_[index];
    const auto& next = chunks_[index + 1];
    if (!current.used && !next.used && current.region == next.region &&
        current.address + current.span_bytes == next.address) {
      current.span_bytes += next.span_bytes;
      chunks_.erase(chunks_.begin() + static_cast<std::ptrdiff_t>(index + 1));
      continue;
    }
    ++index;
  }
}

}  // namespace sim::lpc_memory
