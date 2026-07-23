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

#include "sim/lpc_memory_proto.hpp"

#include <cstdint>

namespace sim::api {
namespace {

carvera::sim::v1::MemoryRegion proto_region(lpc_memory::MemoryRegion region) {
  return region == lpc_memory::MemoryRegion::MainSram ? carvera::sim::v1::MEMORY_REGION_MAIN_SRAM
                                                      : carvera::sim::v1::MEMORY_REGION_AHB_SRAM;
}

}  // namespace

void fill_memory_summary(carvera::sim::v1::MemorySummary& target, const lpc_memory::MemoryAccountingSnapshot& source) {
  target.Clear();
  auto* main = target.mutable_main();
  main->set_capacity_bytes(source.main.capacity_bytes);
  main->set_static_bytes(source.main.static_bytes);
  main->set_stack_reserved_bytes(source.main.stack_reserved_bytes);
  main->set_heap_committed_bytes(source.main.heap_committed_bytes);
  main->set_live_payload_bytes(source.main.live_payload_bytes);
  main->set_peak_live_payload_bytes(source.main.peak_live_payload_bytes);
  main->set_allocator_overhead_bytes(source.main.allocator_overhead_bytes);
  main->set_fragmented_free_bytes(source.main.fragmented_free_bytes);
  main->set_largest_free_block_bytes(source.main.largest_free_block_bytes);
  main->set_top_unallocated_bytes(source.main.top_unallocated_bytes);
  main->set_minimum_margin_bytes(source.main.minimum_margin_bytes);
  main->set_config_cache_active(source.main.config_cache_active);
  main->set_config_cache_start(source.main.config_cache_start);
  main->set_config_cache_bytes(source.main.config_cache_bytes);
  main->set_config_cache_collision(source.main.config_cache_collision);
  main->set_failed_allocation_count(source.main.failed_allocation_count);
  main->set_failed_allocation_bytes(source.main.failed_allocation_bytes);
  main->set_heap_limit_collision(source.main.heap_limit_collision);
  main->set_total_free_bytes(source.main.fragmented_free_bytes + source.main.top_unallocated_bytes);

  auto* ahb = target.mutable_ahb();
  ahb->set_capacity_bytes(source.ahb.capacity_bytes);
  ahb->set_static_bytes(source.ahb.static_bytes);
  ahb->set_dynamic_capacity_bytes(source.ahb.dynamic_capacity_bytes);
  ahb->set_live_payload_bytes(source.ahb.live_payload_bytes);
  ahb->set_peak_live_payload_bytes(source.ahb.peak_live_payload_bytes);
  ahb->set_allocator_overhead_bytes(source.ahb.allocator_overhead_bytes);
  ahb->set_total_free_bytes(source.ahb.total_free_bytes);
  ahb->set_largest_free_block_bytes(source.ahb.largest_free_block_bytes);
  ahb->set_failed_allocation_count(source.ahb.failed_allocation_count);
  ahb->set_failed_allocation_bytes(source.ahb.failed_allocation_bytes);

  for (const auto& group : source.allocation_groups) {
    if (group.target_size_exact) {
      continue;
    }
    const auto live = static_cast<std::uint64_t>(group.host_payload_bytes) * group.live_count;
    const auto peak = static_cast<std::uint64_t>(group.host_payload_bytes) * group.peak_live_count;
    if (group.region == lpc_memory::MemoryRegion::MainSram) {
      target.set_unresolved_main_live_host_bytes(target.unresolved_main_live_host_bytes() + live);
      target.set_unresolved_main_peak_host_bytes(target.unresolved_main_peak_host_bytes() + peak);
    } else {
      target.set_unresolved_ahb_live_host_bytes(target.unresolved_ahb_live_host_bytes() + live);
      target.set_unresolved_ahb_peak_host_bytes(target.unresolved_ahb_peak_host_bytes() + peak);
    }
  }
}

void fill_memory_details(carvera::sim::v1::MemoryDetails& target, const lpc_memory::MemoryAccountingSnapshot& source) {
  target.Clear();
  fill_memory_summary(*target.mutable_summary(), source);
  for (const auto& group : source.allocation_groups) {
    auto* output = target.add_allocation_groups();
    output->set_region(proto_region(group.region));
    output->set_type_name(group.type_name);
    output->set_host_payload_bytes(group.host_payload_bytes);
    output->set_target_payload_bytes(group.target_payload_bytes);
    output->set_live_count(group.live_count);
    output->set_peak_live_count(group.peak_live_count);
    output->set_total_count(group.total_count);
    output->set_live_target_bytes(group.live_target_bytes);
    output->set_peak_target_bytes(group.peak_target_bytes);
    output->set_target_size_exact(group.target_size_exact);
  }
}

}  // namespace sim::api
