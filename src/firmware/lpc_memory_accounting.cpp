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
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint32_t kMainHeapHeaderBytes = 8;
constexpr std::uint32_t kMainHeapAlignment = 8;
constexpr std::uint32_t kAhbHeaderBytes = 4;
constexpr std::uint32_t kAhbAlignment = 4;

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
    result.total_free_bytes += free_payload;
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
