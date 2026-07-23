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

#include "ConfigCache.h"
#include "StreamOutput.h"
#include "compat/active_context.hpp"
#include "lpc_memory_layout.hpp"
#include "sim/simulator_context.hpp"

namespace {

constexpr std::uint32_t kMainHeapHeaderBytes = 8;
constexpr std::uint32_t kMainHeapAlignment = 8;
constexpr std::uint32_t kAhbHeaderBytes = 4;
constexpr std::uint32_t kAhbAlignment = 4;

struct FirmwareCallStack {
  static constexpr std::size_t kMaximumTrackedDepth = 128;

  std::array<void*, kMaximumTrackedDepth> functions{};
  std::size_t depth{};
  void* pending_config_cache_owner{};
  std::size_t pending_config_cache_depth{};
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

std::string describe_firmware_function(void* function_address);
bool is_allocation_runtime_function(std::string_view function_name);

MainSramLayout firmware_main_sram_layout() {
  return {
      .ram_start = generated::kRamStart,
      .ram_end = generated::kRamEnd,
      .static_end = generated::kStaticEnd,
      .stack_top = generated::kStackTop,
      .stack_limit = generated::kStackLimit,
      .heap_limit = generated::kHeapLimit,
      .config_cache_bytes = generated::kConfigCacheBytes,
  };
}

AhbLayout firmware_ahb_layout() {
  return {
      .region_start = generated::kAhbRegionStart,
      .region_end = generated::kAhbRegionEnd,
      .dynamic_start = generated::kAhbDynamicStart,
  };
}

MemoryAccounting::MemoryAccounting() : main_(firmware_main_sram_layout()), ahb_(firmware_ahb_layout()) {}

void MemoryAccounting::record_main(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                                   std::string type_name, bool target_size_exact) {
  record(MemoryRegion::MainSram, pointer, host_payload_bytes, target_payload_bytes, std::move(type_name),
         target_size_exact);
}

void MemoryAccounting::record_ahb(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                                  std::string type_name, bool target_size_exact) {
  record(MemoryRegion::AhbSram, pointer, host_payload_bytes, target_payload_bytes, std::move(type_name),
         target_size_exact);
}

void MemoryAccounting::record(MemoryRegion region, void* pointer, std::size_t host_payload_bytes,
                              std::size_t target_payload_bytes, std::string type_name, bool target_size_exact) {
  if (pointer == nullptr) {
    return;
  }

  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  if (allocations_.contains(pointer)) {
    throw std::invalid_argument("host pointer is already tracked by LPC memory accounting");
  }

  std::size_t group_index = groups_.size();
  for (std::size_t index = 0; index < groups_.size(); ++index) {
    const auto& group = groups_[index];
    if (group.region == region && group.type_name == type_name && group.host_payload_bytes == host_payload_bytes &&
        group.target_payload_bytes == target_payload_bytes && group.target_size_exact == target_size_exact) {
      group_index = index;
      break;
    }
  }
  if (group_index == groups_.size()) {
    groups_.push_back(AllocationGroupSnapshot{
        .region = region,
        .type_name = std::move(type_name),
        .host_payload_bytes = checked_size(host_payload_bytes),
        .target_payload_bytes = checked_size(target_payload_bytes),
        .target_size_exact = target_size_exact,
    });
  }

  const AllocationId id = next_id_++;
  bool modeled = false;
  if (target_size_exact) {
    modeled = region == MemoryRegion::MainSram ? main_.allocate(id, target_payload_bytes)
                                               : ahb_.allocate(id, target_payload_bytes);
  }
  const bool config_cache_owner = region == MemoryRegion::MainSram && groups_[group_index].type_name == "ConfigCache";
  allocations_.emplace(pointer, Allocation{
                                    .region = region,
                                    .id = id,
                                    .group_index = group_index,
                                    .modeled = modeled,
                                    .config_cache_owner = config_cache_owner,
                                });

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
    if (allocation.region == MemoryRegion::MainSram) {
      main_.deallocate(allocation.id);
    } else {
      ahb_.deallocate(allocation.id);
    }
  }
  auto& group = groups_[allocation.group_index];
  --group.live_count;
  group.live_target_bytes -= group.target_payload_bytes;
  if (allocation.config_cache_owner) {
    main_.set_config_cache_active(false);
  }
  allocations_.erase(found);
  return allocation.region;
}

void MemoryAccounting::mark_config_cache_owner(void* pointer, std::size_t target_payload_bytes) {
  if (pointer == nullptr) {
    return;
  }
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  const auto found = allocations_.find(pointer);
  if (found == allocations_.end() || found->second.region != MemoryRegion::MainSram ||
      found->second.config_cache_owner) {
    return;
  }

  auto& allocation = found->second;
  auto& old_group = groups_[allocation.group_index];
  if (allocation.modeled) {
    main_.deallocate(allocation.id);
  }
  --old_group.live_count;
  old_group.live_target_bytes -= old_group.target_payload_bytes;

  std::size_t group_index = groups_.size();
  for (std::size_t index = 0; index < groups_.size(); ++index) {
    const auto& group = groups_[index];
    if (group.region == MemoryRegion::MainSram && group.type_name == "ConfigCache" &&
        group.host_payload_bytes == old_group.host_payload_bytes &&
        group.target_payload_bytes == target_payload_bytes && group.target_size_exact) {
      group_index = index;
      break;
    }
  }
  if (group_index == groups_.size()) {
    groups_.push_back(AllocationGroupSnapshot{
        .region = MemoryRegion::MainSram,
        .type_name = "ConfigCache",
        .host_payload_bytes = old_group.host_payload_bytes,
        .target_payload_bytes = checked_size(target_payload_bytes),
        .target_size_exact = true,
    });
  }

  auto& group = groups_[group_index];
  ++group.live_count;
  ++group.total_count;
  group.peak_live_count = std::max(group.peak_live_count, group.live_count);
  group.live_target_bytes += target_payload_bytes;
  group.peak_target_bytes = std::max(group.peak_target_bytes, group.live_target_bytes);
  allocation.group_index = group_index;
  allocation.modeled = main_.allocate(allocation.id, target_payload_bytes);
  allocation.config_cache_owner = true;
}

void MemoryAccounting::set_config_cache_active(bool active) {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  main_.set_config_cache_active(active);
}

void MemoryAccounting::release_config_cache() {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  for (auto allocation = allocations_.begin(); allocation != allocations_.end(); ++allocation) {
    if (!allocation->second.config_cache_owner) {
      continue;
    }
    if (allocation->second.modeled) {
      main_.deallocate(allocation->second.id);
    }
    auto& group = groups_[allocation->second.group_index];
    --group.live_count;
    group.live_target_bytes -= group.target_payload_bytes;
    allocations_.erase(allocation);
    break;
  }
  main_.set_config_cache_active(false);
}

void MemoryAccounting::reset() {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  main_.reset();
  ahb_.reset();
  allocations_.clear();
  groups_.clear();
  next_id_ = 1;
}

MemoryAccountingSnapshot MemoryAccounting::snapshot() const {
  AccountingSuppression suppression;
  std::scoped_lock lock(mutex_);
  return {
      .main = main_.snapshot(),
      .ahb = ahb_.snapshot(),
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
  if (firmware_calls.pending_config_cache_owner != nullptr &&
      firmware_calls.depth < firmware_calls.pending_config_cache_depth) {
    firmware_calls.pending_config_cache_owner = nullptr;
  }
}

bool firmware_allocation_active() noexcept { return firmware_calls.depth > 0 && !firmware_calls.accounting_suppressed; }

void config_cache_storage_acquired() noexcept {
  AccountingSuppression suppression;
  void* owner = firmware_calls.pending_config_cache_owner;
  firmware_calls.pending_config_cache_owner = nullptr;
  try {
    if (auto* context = compat::try_active_context(); context != nullptr) {
      context->memory_accounting().mark_config_cache_owner(owner, generated::kConfigCacheObjectBytes);
      context->memory_accounting().set_config_cache_active(true);
    }
  } catch (...) {
  }
}

void record_host_main_allocation(void* pointer, std::size_t host_payload_bytes, bool array_allocation) noexcept {
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
        resolve_generic_main_allocation(host_payload_bytes, array_allocation, origin, allocation_implementation);
    context->memory_accounting().record_main(pointer, host_payload_bytes, resolved.target_payload_bytes,
                                             std::move(resolved.type_name), resolved.target_size_exact);
    if (host_payload_bytes == sizeof(ConfigCache)) {
      firmware_calls.pending_config_cache_owner = pointer;
      firmware_calls.pending_config_cache_depth = firmware_calls.depth;
    }
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
      context->memory_accounting().record_main(copy, bytes, bytes, "strdup char[]", true);
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
  const auto& main = report.main;
  stream->printf("LPC1768 Main SRAM: capacity=%lu static=%lu stack=%lu\n",
                 static_cast<unsigned long>(main.capacity_bytes), static_cast<unsigned long>(main.static_bytes),
                 static_cast<unsigned long>(main.stack_reserved_bytes));
  stream->printf(
      "  Heap: committed=%lu live=%lu peak=%lu overhead=%lu fragmented=%lu top-free=%lu\n",
      static_cast<unsigned long>(main.heap_committed_bytes), static_cast<unsigned long>(main.live_payload_bytes),
      static_cast<unsigned long>(main.peak_live_payload_bytes),
      static_cast<unsigned long>(main.allocator_overhead_bytes), static_cast<unsigned long>(main.fragmented_free_bytes),
      static_cast<unsigned long>(main.top_unallocated_bytes));
  stream->printf("  Total Free RAM (Main Heap): %lu bytes; minimum-margin=%lu\n",
                 static_cast<unsigned long>(main.fragmented_free_bytes + main.top_unallocated_bytes),
                 static_cast<unsigned long>(main.minimum_margin_bytes));
  stream->printf("  Config cache: %s start=0x%08lX size=%lu collision=%s\n",
                 main.config_cache_active ? "active" : "released", static_cast<unsigned long>(main.config_cache_start),
                 static_cast<unsigned long>(main.config_cache_bytes), main.config_cache_collision ? "yes" : "no");
  stream->printf("  Main allocation failures: %lu (%llu requested bytes); heap-limit-collision=%s\n",
                 static_cast<unsigned long>(main.failed_allocation_count),
                 static_cast<unsigned long long>(main.failed_allocation_bytes),
                 main.heap_limit_collision ? "yes" : "no");

  const auto& ahb = report.ahb;
  stream->printf("LPC1768 AHB SRAM: capacity=%lu static=%lu dynamic=%lu\n",
                 static_cast<unsigned long>(ahb.capacity_bytes), static_cast<unsigned long>(ahb.static_bytes),
                 static_cast<unsigned long>(ahb.dynamic_capacity_bytes));
  stream->printf(
      "  Pool: live=%lu peak=%lu overhead=%lu free=%lu largest-free=%lu\n",
      static_cast<unsigned long>(ahb.live_payload_bytes), static_cast<unsigned long>(ahb.peak_live_payload_bytes),
      static_cast<unsigned long>(ahb.allocator_overhead_bytes), static_cast<unsigned long>(ahb.total_free_bytes),
      static_cast<unsigned long>(ahb.largest_free_block_bytes));
  stream->printf("  AHB allocation failures: %lu (%llu requested bytes)\n",
                 static_cast<unsigned long>(ahb.failed_allocation_count),
                 static_cast<unsigned long long>(ahb.failed_allocation_bytes));

  std::uint64_t unresolved_main_live = 0;
  std::uint64_t unresolved_main_peak = 0;
  std::uint64_t unresolved_ahb_live = 0;
  std::uint64_t unresolved_ahb_peak = 0;
  for (const auto& group : report.allocation_groups) {
    if (group.target_size_exact) {
      continue;
    }
    auto& live = group.region == MemoryRegion::MainSram ? unresolved_main_live : unresolved_ahb_live;
    auto& peak = group.region == MemoryRegion::MainSram ? unresolved_main_peak : unresolved_ahb_peak;
    live += static_cast<std::uint64_t>(group.host_payload_bytes) * group.live_count;
    peak += static_cast<std::uint64_t>(group.host_payload_bytes) * group.peak_live_count;
  }
  stream->printf(
      "Unresolved ABI allocations (not charged to LPC totals): main live=%llu peak=%llu; "
      "AHB live=%llu peak=%llu host-request bytes\n",
      static_cast<unsigned long long>(unresolved_main_live), static_cast<unsigned long long>(unresolved_main_peak),
      static_cast<unsigned long long>(unresolved_ahb_live), static_cast<unsigned long long>(unresolved_ahb_peak));

  if (!verbose) {
    return;
  }
  std::ranges::sort(report.allocation_groups, [](const auto& lhs, const auto& rhs) {
    if (lhs.region != rhs.region) {
      return lhs.region < rhs.region;
    }
    return lhs.peak_target_bytes > rhs.peak_target_bytes;
  });
  stream->printf("Allocation groups (host request -> LPC charge):\n");
  for (const auto& group : report.allocation_groups) {
    const char* region = group.region == MemoryRegion::MainSram ? "main" : "AHB";
    const char* type = group.type_name.empty() ? "unlabelled" : group.type_name.c_str();
    stream->printf("  %s %-20s %lu -> %lu bytes, live=%lu peak=%lu total=%lu%s\n", region, type,
                   static_cast<unsigned long>(group.host_payload_bytes),
                   static_cast<unsigned long>(group.target_payload_bytes), static_cast<unsigned long>(group.live_count),
                   static_cast<unsigned long>(group.peak_live_count), static_cast<unsigned long>(group.total_count),
                   group.target_size_exact ? "" : " (host-size estimate)");
  }
}

MainSramModel::MainSramModel(MainSramLayout layout) : layout_(layout) {
  if (!(layout_.ram_start <= layout_.static_end && layout_.static_end <= layout_.heap_limit &&
        layout_.heap_limit <= layout_.stack_limit && layout_.stack_limit <= layout_.stack_top &&
        layout_.stack_top <= layout_.ram_end)) {
    throw std::invalid_argument("invalid LPC main SRAM layout");
  }
  if (layout_.config_cache_bytes > layout_.stack_limit - layout_.ram_start) {
    throw std::invalid_argument("config cache does not fit below StackLimit");
  }
  reset();
}

bool MainSramModel::allocate(AllocationId id, std::size_t target_payload_bytes) {
  if (allocation_chunks_.contains(id)) {
    throw std::invalid_argument("duplicate LPC main-heap allocation id");
  }

  const auto payload = checked_size(target_payload_bytes);
  const auto required = aligned_span(target_payload_bytes, kMainHeapHeaderBytes, kMainHeapAlignment);
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    if (chunks_[index].used || chunks_[index].span_bytes < required) {
      continue;
    }

    const auto remaining = chunks_[index].span_bytes - required;
    if (remaining >= kMainHeapHeaderBytes + kMainHeapAlignment) {
      const Chunk tail{
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
    rebuild_allocation_index(chunks_, allocation_chunks_);
    update_margin();
    return true;
  }

  if (required > active_heap_limit() - std::min(heap_break_, active_heap_limit())) {
    ++failed_allocation_count_;
    failed_allocation_bytes_ += payload;
    if (config_cache_active_) {
      config_cache_collision_ = true;
    } else {
      heap_limit_collision_ = true;
    }
    return false;
  }

  chunks_.push_back(Chunk{
      .address = heap_break_,
      .span_bytes = required,
      .payload_bytes = payload,
      .allocation_id = id,
      .used = true,
  });
  allocation_chunks_[id] = chunks_.size() - 1;
  heap_break_ += required;
  live_payload_bytes_ += payload;
  peak_live_payload_bytes_ = std::max(peak_live_payload_bytes_, live_payload_bytes_);
  update_margin();
  return true;
}

void MainSramModel::deallocate(AllocationId id) {
  const auto found = allocation_chunks_.find(id);
  if (found == allocation_chunks_.end()) {
    return;
  }
  auto& chunk = chunks_[found->second];
  live_payload_bytes_ -= chunk.payload_bytes;
  chunk.payload_bytes = 0;
  chunk.allocation_id = 0;
  chunk.used = false;
  coalesce_free_chunks();
  rebuild_allocation_index(chunks_, allocation_chunks_);
}

void MainSramModel::set_config_cache_active(bool active) {
  config_cache_active_ = active;
  if (heap_break_ > active_heap_limit()) {
    if (config_cache_active_) {
      config_cache_collision_ = true;
    } else {
      heap_limit_collision_ = true;
    }
  }
  update_margin();
}

void MainSramModel::reset() {
  chunks_.clear();
  allocation_chunks_.clear();
  heap_break_ = layout_.static_end;
  live_payload_bytes_ = 0;
  peak_live_payload_bytes_ = 0;
  minimum_margin_bytes_ = std::numeric_limits<std::uint32_t>::max();
  failed_allocation_count_ = 0;
  failed_allocation_bytes_ = 0;
  config_cache_active_ = false;
  config_cache_collision_ = false;
  heap_limit_collision_ = false;
  update_margin();
}

MainSramSnapshot MainSramModel::snapshot() const {
  MainSramSnapshot result{
      .capacity_bytes = layout_.ram_end - layout_.ram_start,
      .static_bytes = layout_.static_end - layout_.ram_start,
      .stack_reserved_bytes = layout_.stack_top - layout_.stack_limit,
      .heap_break = heap_break_,
      .active_heap_limit = active_heap_limit(),
      .heap_committed_bytes = heap_break_ - layout_.static_end,
      .live_payload_bytes = live_payload_bytes_,
      .peak_live_payload_bytes = peak_live_payload_bytes_,
      .top_unallocated_bytes = active_heap_limit() > heap_break_ ? active_heap_limit() - heap_break_ : 0,
      .minimum_margin_bytes = minimum_margin_bytes_,
      .config_cache_start = layout_.stack_limit - layout_.config_cache_bytes,
      .config_cache_bytes = layout_.config_cache_bytes,
      .failed_allocation_count = failed_allocation_count_,
      .failed_allocation_bytes = failed_allocation_bytes_,
      .config_cache_active = config_cache_active_,
      .config_cache_collision = config_cache_collision_,
      .heap_limit_collision = heap_limit_collision_,
  };

  for (const auto& chunk : chunks_) {
    if (chunk.used) {
      result.allocator_overhead_bytes += chunk.span_bytes - chunk.payload_bytes;
      continue;
    }
    const auto free_payload = chunk.span_bytes > kMainHeapHeaderBytes ? chunk.span_bytes - kMainHeapHeaderBytes : 0;
    result.fragmented_free_bytes += free_payload;
    result.largest_free_block_bytes = std::max(result.largest_free_block_bytes, free_payload);
  }
  return result;
}

std::uint32_t MainSramModel::active_heap_limit() const {
  if (!config_cache_active_) {
    return layout_.heap_limit;
  }
  return std::min(layout_.heap_limit, layout_.stack_limit - layout_.config_cache_bytes);
}

void MainSramModel::coalesce_free_chunks() {
  for (std::size_t index = 0; index + 1 < chunks_.size();) {
    auto& current = chunks_[index];
    const auto& next = chunks_[index + 1];
    if (!current.used && !next.used && current.address + current.span_bytes == next.address) {
      current.span_bytes += next.span_bytes;
      chunks_.erase(chunks_.begin() + static_cast<std::ptrdiff_t>(index + 1));
      continue;
    }
    ++index;
  }
}

void MainSramModel::update_margin() {
  const auto limit = active_heap_limit();
  const auto margin = limit > heap_break_ ? limit - heap_break_ : 0;
  minimum_margin_bytes_ = std::min(minimum_margin_bytes_, margin);
}

AhbPoolModel::AhbPoolModel(AhbLayout layout) : layout_(layout) {
  if (!(layout_.region_start <= layout_.dynamic_start && layout_.dynamic_start <= layout_.region_end)) {
    throw std::invalid_argument("invalid LPC AHB SRAM layout");
  }
  reset();
}

bool AhbPoolModel::allocate(AllocationId id, std::size_t target_payload_bytes) {
  if (allocation_chunks_.contains(id)) {
    throw std::invalid_argument("duplicate LPC AHB allocation id");
  }

  const auto payload = checked_size(target_payload_bytes);
  const auto required = aligned_span(target_payload_bytes, kAhbHeaderBytes, kAhbAlignment);
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    if (chunks_[index].used || chunks_[index].span_bytes < required) {
      continue;
    }

    const auto remaining = chunks_[index].span_bytes - required;
    if (remaining >= kAhbHeaderBytes + kAhbAlignment) {
      const Chunk tail{
          .offset = chunks_[index].offset + required,
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
    rebuild_allocation_index(chunks_, allocation_chunks_);
    return true;
  }

  ++failed_allocation_count_;
  failed_allocation_bytes_ += payload;
  return false;
}

void AhbPoolModel::deallocate(AllocationId id) {
  const auto found = allocation_chunks_.find(id);
  if (found == allocation_chunks_.end()) {
    return;
  }
  auto& chunk = chunks_[found->second];
  live_payload_bytes_ -= chunk.payload_bytes;
  chunk.payload_bytes = 0;
  chunk.allocation_id = 0;
  chunk.used = false;
  coalesce_free_chunks();
  rebuild_allocation_index(chunks_, allocation_chunks_);
}

void AhbPoolModel::reset() {
  chunks_.clear();
  allocation_chunks_.clear();
  live_payload_bytes_ = 0;
  peak_live_payload_bytes_ = 0;
  failed_allocation_count_ = 0;
  failed_allocation_bytes_ = 0;
  const auto capacity = layout_.region_end - layout_.dynamic_start;
  if (capacity >= kAhbHeaderBytes) {
    chunks_.push_back(Chunk{
        .span_bytes = capacity,
    });
  }
}

AhbPoolSnapshot AhbPoolModel::snapshot() const {
  AhbPoolSnapshot result{
      .capacity_bytes = layout_.region_end - layout_.region_start,
      .static_bytes = layout_.dynamic_start - layout_.region_start,
      .dynamic_capacity_bytes = layout_.region_end - layout_.dynamic_start,
      .live_payload_bytes = live_payload_bytes_,
      .peak_live_payload_bytes = peak_live_payload_bytes_,
      .failed_allocation_count = failed_allocation_count_,
      .failed_allocation_bytes = failed_allocation_bytes_,
  };
  for (const auto& chunk : chunks_) {
    if (chunk.used) {
      result.allocator_overhead_bytes += chunk.span_bytes - chunk.payload_bytes;
      continue;
    }
    const auto free_payload = chunk.span_bytes > kAhbHeaderBytes ? chunk.span_bytes - kAhbHeaderBytes : 0;
    // Match MemoryPool::free(): the target firmware counts the entire span of
    // every free block, including the header which will be reused on allocation.
    result.total_free_bytes += chunk.span_bytes;
    result.largest_free_block_bytes = std::max(result.largest_free_block_bytes, free_payload);
  }
  return result;
}

void AhbPoolModel::coalesce_free_chunks() {
  for (std::size_t index = 0; index + 1 < chunks_.size();) {
    auto& current = chunks_[index];
    const auto& next = chunks_[index + 1];
    if (!current.used && !next.used && current.offset + current.span_bytes == next.offset) {
      current.span_bytes += next.span_bytes;
      chunks_.erase(chunks_.begin() + static_cast<std::ptrdiff_t>(index + 1));
      continue;
    }
    ++index;
  }
}

}  // namespace sim::lpc_memory
