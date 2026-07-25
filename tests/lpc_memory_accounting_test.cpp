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

#include "support/assertions.hpp"

#include "CartesianSolution.h"
#include "sim/lpc_memory_accounting.hpp"

namespace {

using sim::lpc_memory::AhbLayout;
using sim::lpc_memory::AhbPoolModel;
using sim::lpc_memory::firmware_ahb_layout;
using sim::lpc_memory::firmware_main_sram_layout;
using sim::lpc_memory::MainSramLayout;
using sim::lpc_memory::MainSramModel;
using sim::lpc_memory::MemoryAccounting;
using sim::lpc_memory::MemoryRegion;
using sim::test::require;

void generated_layout_matches_the_pinned_arm_firmware() {
  const auto main = firmware_main_sram_layout();
  require(main.ram_start == 0x100000c8, "main RAM origin should come from the ARM linker map");
  require(main.ram_end == 0x10008000, "main RAM end should come from the ARM linker map");
  require(main.static_end == 0x10000fd0, "static RAM end should come from the linked image");
  require(main.heap_limit == 0x10006fe0, "heap limit should include the firmware's MPU guard");
  require(main.config_cache_bytes == 9'100, "config cache should use the ARM ConfigValue layout");

  const auto ahb = firmware_ahb_layout();
  require(ahb.region_start == 0x2007c000, "AHB RAM origin should come from the ARM linker map");
  require(ahb.region_end == 0x20084000, "AHB RAM end should come from the ARM linker map");
  require(ahb.dynamic_start == 0x2007fca8, "dynamic AHB start should follow linked static AHB data");
  require(ahb.region_end - ahb.dynamic_start == 17'240, "dynamic AHB capacity should match the pinned image");
}

void main_sram_tracks_temporary_config_cache_collisions() {
  MainSramModel memory(MainSramLayout{
      .ram_start = 0x10000000,
      .ram_end = 0x10008000,
      .static_end = 0x10002000,
      .stack_top = 0x10008000,
      .stack_limit = 0x10007000,
      .heap_limit = 0x10006fe0,
      .config_cache_bytes = 0x2000,
  });

  require(memory.allocate(1, 0x400), "initial heap allocation should fit");
  memory.set_config_cache_active(true);
  auto snapshot = memory.snapshot();
  require(snapshot.config_cache_active, "config cache should be active");
  require(snapshot.config_cache_start == 0x10005000, "cache should end at StackLimit");
  require(snapshot.heap_break == 0x10002408, "newlib-style chunk should include and align its header");
  require(snapshot.active_heap_limit == snapshot.config_cache_start,
          "live config cache should temporarily lower the heap limit");
  require(snapshot.minimum_margin_bytes == snapshot.config_cache_start - snapshot.heap_break,
          "report should retain the smallest observed heap-to-cache margin");

  require(!memory.allocate(2, 0x3000), "allocation crossing the config cache should be reported");
  snapshot = memory.snapshot();
  require(snapshot.config_cache_collision, "cache collision should remain visible in the report");
  require(snapshot.failed_allocation_count == 1, "failed shadow allocations should be counted");

  memory.set_config_cache_active(false);
  snapshot = memory.snapshot();
  require(!snapshot.config_cache_active, "config cache reservation should be released after boot");
  require(snapshot.active_heap_limit == 0x10006fe0, "released cache should restore ordinary heap headroom");
  require(snapshot.config_cache_collision, "historical collision should remain visible after release");
}

void main_sram_tracks_fragmentation_without_shrinking_the_break() {
  MainSramModel memory(MainSramLayout{
      .ram_start = 0,
      .ram_end = 4096,
      .static_end = 512,
      .stack_top = 4096,
      .stack_limit = 3584,
      .heap_limit = 3552,
      .config_cache_bytes = 512,
  });

  require(memory.allocate(1, 24), "first allocation should fit");
  require(memory.allocate(2, 40), "second allocation should fit");
  const auto committed = memory.snapshot().heap_committed_bytes;
  memory.deallocate(1);

  auto snapshot = memory.snapshot();
  require(snapshot.heap_committed_bytes == committed, "free should not move the newlib program break backwards");
  require(snapshot.fragmented_free_bytes == 24, "freed payload should be reported as reusable fragmentation");
  require(snapshot.largest_free_block_bytes == 24, "largest free block should describe allocatable payload");

  require(memory.allocate(3, 16), "a smaller allocation should reuse the free chunk");
  snapshot = memory.snapshot();
  require(snapshot.heap_committed_bytes == committed, "reusing a free chunk should not grow the heap");
  require(snapshot.live_payload_bytes == 56, "live payload should exclude allocator metadata and alignment");
  require(snapshot.peak_live_payload_bytes == 64, "peak payload should survive frees and reuse");
}

void ahb_pool_uses_lpc_headers_and_reports_fragmentation() {
  AhbPoolModel memory(AhbLayout{
      .region_start = 0x2007c000,
      .region_end = 0x2007c100,
      .dynamic_start = 0x2007c040,
  });

  require(memory.allocate(1, 64), "first AHB allocation should fit");
  require(memory.allocate(2, 64), "second AHB allocation should fit");
  auto snapshot = memory.snapshot();
  require(snapshot.static_bytes == 64, "linker-placed AHB bytes should be reported separately");
  require(snapshot.dynamic_capacity_bytes == 192, "dynamic capacity should be the post-static interval");
  require(snapshot.live_payload_bytes == 128, "AHB payload should not include pool headers");
  require(snapshot.allocator_overhead_bytes == 8, "each AHB allocation should carry a four-byte header");

  require(!memory.allocate(3, 64), "shadow AHB exhaustion should be reported without changing host allocation");
  snapshot = memory.snapshot();
  require(snapshot.failed_allocation_count == 1, "AHB exhaustion should be retained in the report");

  memory.deallocate(1);
  memory.deallocate(2);
  snapshot = memory.snapshot();
  require(snapshot.total_free_bytes == 192, "AHB free bytes should match the firmware's whole-free-span accounting");
  require(snapshot.largest_free_block_bytes == 188, "adjacent AHB frees should coalesce");
}

void accounting_service_keeps_host_success_separate_from_lpc_capacity() {
  MemoryAccounting memory;
  int main_pointer = 0;
  int ahb_pointer = 0;

  memory.record_main(&main_pointer, 16, 8, "ConfigCache");
  memory.set_config_cache_active(true);
  memory.record_ahb(&ahb_pointer, 32, 20, "TargetType");

  auto snapshot = memory.snapshot();
  require(snapshot.main.config_cache_active, "service should expose the temporary cache reservation");
  require(snapshot.main.live_payload_bytes == 8, "main heap should use the target payload size");
  require(snapshot.ahb.live_payload_bytes == 20, "AHB pool should use the target payload size");
  require(snapshot.allocation_groups.size() == 2, "allocations should be grouped for reporting");
  require(snapshot.allocation_groups[0].host_payload_bytes == 16, "report should retain the host request");
  require(snapshot.allocation_groups[0].target_payload_bytes == 8, "report should retain the LPC charge");

  require(memory.deallocate(&main_pointer) == MemoryRegion::MainSram, "tracked main allocation should be released");
  require(!memory.snapshot().main.config_cache_active, "deleting the cache owner should release its reservation");
  require(memory.deallocate(&ahb_pointer) == MemoryRegion::AhbSram, "tracked AHB allocation should be released");
  require(memory.snapshot().ahb.live_payload_bytes == 0, "released AHB payload should no longer be live");
}

void generic_allocations_are_only_charged_when_the_lpc_layout_is_known() {
  const auto cartesian =
      sim::lpc_memory::resolve_generic_main_allocation(sizeof(CartesianSolution), false, "Robot::load_config()+0x40");
  require(cartesian.target_size_exact, "known firmware object allocation should have an exact LPC charge");
  require(cartesian.type_name == "CartesianSolution", "known allocation should report its firmware type");

  const auto command_buffer =
      sim::lpc_memory::resolve_generic_main_allocation(128, true, "ZProbe::coordinated_move()+0x20");
  require(command_buffer.target_size_exact, "known byte-array allocation should preserve its exact byte count");
  require(command_buffer.target_payload_bytes == 128, "byte arrays should occupy the requested bytes on the LPC");
  require(command_buffer.type_name == "char[]", "known byte-array allocation should report its element type");

  const auto reserved_float_storage = sim::lpc_memory::resolve_generic_main_allocation(
      4096, false, "Endstops::test_endstop_repeatability(Gcode*)",
      "float* std::__1::__libcpp_allocate<float>(std::__1::__element_count, unsigned long)");
  require(reserved_float_storage.target_size_exact,
          "explicitly reserved float-vector storage should have an exact LPC charge");
  require(reserved_float_storage.target_payload_bytes == 4096,
          "float-vector storage should occupy the same bytes on host and LPC");
  require(
      reserved_float_storage.type_name == "std::vector<float> storage @ Endstops::test_endstop_repeatability(Gcode*)",
      "float-vector storage should report the owning firmware function");

  const auto unresolved = sim::lpc_memory::resolve_generic_main_allocation(37, false, "UnknownFirmwareFunction()+0x10");
  require(!unresolved.target_size_exact, "unknown ABI-dependent allocations must remain outside LPC totals");
  require(unresolved.type_name == "ABI-unresolved @ UnknownFirmwareFunction()+0x10",
          "unknown allocations should still identify their firmware origin");
}

}  // namespace

int main() {
  generated_layout_matches_the_pinned_arm_firmware();
  main_sram_tracks_temporary_config_cache_collisions();
  main_sram_tracks_fragmentation_without_shrinking_the_break();
  ahb_pool_uses_lpc_headers_and_reports_fragmentation();
  accounting_service_keeps_host_success_separate_from_lpc_capacity();
  generic_allocations_are_only_charged_when_the_lpc_layout_is_known();
  return 0;
}
