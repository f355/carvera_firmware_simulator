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

#ifndef SIMULATOR_SIM_LPC_MEMORY_ACCOUNTING_HPP
#define SIMULATOR_SIM_LPC_MEMORY_ACCOUNTING_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class StreamOutput;

namespace sim::lpc_memory {

using AllocationId = std::uint64_t;

enum class MemoryRegion {
  MainSram,
  AhbSram,
  UnifiedHeap,
};

struct MainSramLayout {
  std::uint32_t ram_start{};
  std::uint32_t ram_end{};
  std::uint32_t static_end{};
  std::uint32_t stack_top{};
  std::uint32_t stack_limit{};
  std::uint32_t heap_start{};
  std::uint32_t heap_end{};
};

MainSramLayout firmware_main_sram_layout();

struct MainSramSnapshot {
  std::uint32_t capacity_bytes{};
  std::uint32_t static_bytes{};
  std::uint32_t stack_reserved_bytes{};
  std::uint32_t heap_break{};
  std::uint32_t active_heap_limit{};
  std::uint32_t heap_committed_bytes{};
  std::uint32_t live_payload_bytes{};
  std::uint32_t peak_live_payload_bytes{};
  std::uint32_t allocator_overhead_bytes{};
  std::uint32_t fragmented_free_bytes{};
  std::uint32_t largest_free_block_bytes{};
  std::uint32_t top_unallocated_bytes{};
  std::uint32_t minimum_margin_bytes{};
  std::uint32_t config_cache_start{};
  std::uint32_t config_cache_bytes{};
  std::uint32_t failed_allocation_count{};
  std::uint64_t failed_allocation_bytes{};
  bool config_cache_active{};
  bool config_cache_collision{};
  bool heap_limit_collision{};
};

struct AhbLayout {
  std::uint32_t region_start{};
  std::uint32_t region_end{};
  std::uint32_t static_end{};
  std::uint32_t heap_start{};
  std::uint32_t heap_end{};
};

AhbLayout firmware_ahb_layout();

struct AhbPoolSnapshot {
  std::uint32_t capacity_bytes{};
  std::uint32_t static_bytes{};
  std::uint32_t dynamic_capacity_bytes{};
  std::uint32_t live_payload_bytes{};
  std::uint32_t peak_live_payload_bytes{};
  std::uint32_t allocator_overhead_bytes{};
  std::uint32_t total_free_bytes{};
  std::uint32_t largest_free_block_bytes{};
  std::uint32_t failed_allocation_count{};
  std::uint64_t failed_allocation_bytes{};
};

struct UnifiedHeapSnapshot {
  std::uint32_t capacity_bytes{};
  std::uint32_t live_payload_bytes{};
  std::uint32_t peak_live_payload_bytes{};
  std::uint32_t allocator_overhead_bytes{};
  std::uint32_t total_free_bytes{};
  std::uint32_t minimum_ever_free_bytes{};
  std::uint32_t largest_free_block_bytes{};
  std::uint32_t smallest_free_block_bytes{};
  std::uint32_t free_area_count{};
  std::uint32_t successful_allocation_count{};
  std::uint32_t successful_free_count{};
  std::uint32_t failed_allocation_count{};
  std::uint64_t failed_allocation_bytes{};
};

struct UnifiedHeapModelSnapshot {
  UnifiedHeapSnapshot heap;
  MainSramSnapshot main;
  AhbPoolSnapshot ahb;
};

class UnifiedHeapModel {
 public:
  UnifiedHeapModel(MainSramLayout main_layout, AhbLayout ahb_layout);

  std::optional<MemoryRegion> allocate(AllocationId id, std::size_t target_payload_bytes);
  std::optional<MemoryRegion> deallocate(AllocationId id);
  void reset();

  UnifiedHeapModelSnapshot snapshot() const;

 private:
  struct Chunk {
    MemoryRegion region{};
    std::uint32_t address{};
    std::uint32_t span_bytes{};
    std::uint32_t payload_bytes{};
    AllocationId allocation_id{};
    bool used{};
  };

  void coalesce_free_chunks();

  MainSramLayout main_layout_;
  AhbLayout ahb_layout_;
  std::vector<Chunk> chunks_;
  std::unordered_map<AllocationId, std::size_t> allocation_chunks_;
  std::uint32_t live_payload_bytes_{};
  std::uint32_t peak_live_payload_bytes_{};
  std::uint32_t main_live_payload_bytes_{};
  std::uint32_t main_peak_live_payload_bytes_{};
  std::uint32_t ahb_live_payload_bytes_{};
  std::uint32_t ahb_peak_live_payload_bytes_{};
  std::uint32_t minimum_ever_free_bytes_{};
  std::uint32_t successful_allocation_count_{};
  std::uint32_t successful_free_count_{};
  std::uint32_t failed_allocation_count_{};
  std::uint64_t failed_allocation_bytes_{};
};

struct AllocationGroupSnapshot {
  MemoryRegion region{};
  std::string type_name;
  std::uint32_t host_payload_bytes{};
  std::uint32_t target_payload_bytes{};
  std::uint32_t live_count{};
  std::uint32_t peak_live_count{};
  std::uint32_t total_count{};
  std::uint64_t live_target_bytes{};
  std::uint64_t peak_target_bytes{};
  bool target_size_exact{};
};

struct MemoryAccountingSnapshot {
  UnifiedHeapSnapshot heap;
  MainSramSnapshot main;
  AhbPoolSnapshot ahb;
  std::vector<AllocationGroupSnapshot> allocation_groups;
};

struct ResolvedAllocation {
  std::size_t target_payload_bytes{};
  std::string type_name;
  bool target_size_exact{};
};

ResolvedAllocation resolve_generic_heap_allocation(std::size_t host_payload_bytes, bool array_allocation,
                                                   std::string_view origin,
                                                   std::string_view allocation_implementation = {});

std::string describe_firmware_function(void* function_address);
bool is_allocation_runtime_function(std::string_view function_name);

class MemoryAccounting {
 public:
  MemoryAccounting();

  void record_heap(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes,
                   std::string type_name = {}, bool target_size_exact = true);
  std::optional<MemoryRegion> deallocate(void* pointer);
  void release_config_cache();
  void reset();

  MemoryAccountingSnapshot snapshot() const;

 private:
  struct Allocation {
    MemoryRegion region{};
    AllocationId id{};
    std::size_t group_index{};
    bool modeled{};
  };

  void record(void* pointer, std::size_t host_payload_bytes, std::size_t target_payload_bytes, std::string type_name,
              bool target_size_exact);
  std::size_t find_or_create_group(MemoryRegion region, std::string type_name, std::size_t host_payload_bytes,
                                   std::size_t target_payload_bytes, bool target_size_exact);
  void charge_group(std::size_t group_index, std::size_t target_payload_bytes);

  mutable std::mutex mutex_;
  UnifiedHeapModel heap_;
  std::unordered_map<void*, Allocation> allocations_;
  std::vector<AllocationGroupSnapshot> groups_;
  AllocationId next_id_{1};
};

void enter_firmware_function(void* function_address) noexcept;
void exit_firmware_function() noexcept;
bool firmware_allocation_active() noexcept;
void record_host_heap_allocation(void* pointer, std::size_t host_payload_bytes, bool array_allocation) noexcept;
bool release_host_allocation(void* pointer) noexcept;
char* tracked_strdup(const char* source) noexcept;
void tracked_free(void* pointer) noexcept;
void print_memory_report(StreamOutput* stream, bool verbose);

}  // namespace sim::lpc_memory

#endif
