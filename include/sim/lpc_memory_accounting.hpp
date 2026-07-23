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
#include <unordered_map>
#include <vector>

namespace sim::lpc_memory {

using AllocationId = std::uint64_t;

struct MainSramLayout {
  std::uint32_t ram_start{};
  std::uint32_t ram_end{};
  std::uint32_t static_end{};
  std::uint32_t stack_top{};
  std::uint32_t stack_limit{};
  std::uint32_t heap_limit{};
  std::uint32_t config_cache_bytes{};
};

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

class MainSramModel {
 public:
  explicit MainSramModel(MainSramLayout layout);

  bool allocate(AllocationId id, std::size_t target_payload_bytes);
  void deallocate(AllocationId id);
  void set_config_cache_active(bool active);
  void reset();

  MainSramSnapshot snapshot() const;

 private:
  struct Chunk {
    std::uint32_t address{};
    std::uint32_t span_bytes{};
    std::uint32_t payload_bytes{};
    AllocationId allocation_id{};
    bool used{};
  };

  std::uint32_t active_heap_limit() const;
  void coalesce_free_chunks();
  void update_margin();

  MainSramLayout layout_;
  std::vector<Chunk> chunks_;
  std::unordered_map<AllocationId, std::size_t> allocation_chunks_;
  std::uint32_t heap_break_{};
  std::uint32_t live_payload_bytes_{};
  std::uint32_t peak_live_payload_bytes_{};
  std::uint32_t minimum_margin_bytes_{};
  std::uint32_t failed_allocation_count_{};
  std::uint64_t failed_allocation_bytes_{};
  bool config_cache_active_{};
  bool config_cache_collision_{};
  bool heap_limit_collision_{};
};

struct AhbLayout {
  std::uint32_t region_start{};
  std::uint32_t region_end{};
  std::uint32_t dynamic_start{};
};

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

class AhbPoolModel {
 public:
  explicit AhbPoolModel(AhbLayout layout);

  bool allocate(AllocationId id, std::size_t target_payload_bytes);
  void deallocate(AllocationId id);
  void reset();

  AhbPoolSnapshot snapshot() const;

 private:
  struct Chunk {
    std::uint32_t offset{};
    std::uint32_t span_bytes{};
    std::uint32_t payload_bytes{};
    AllocationId allocation_id{};
    bool used{};
  };

  void coalesce_free_chunks();

  AhbLayout layout_;
  std::vector<Chunk> chunks_;
  std::unordered_map<AllocationId, std::size_t> allocation_chunks_;
  std::uint32_t live_payload_bytes_{};
  std::uint32_t peak_live_payload_bytes_{};
  std::uint32_t failed_allocation_count_{};
  std::uint64_t failed_allocation_bytes_{};
};

}  // namespace sim::lpc_memory

#endif
