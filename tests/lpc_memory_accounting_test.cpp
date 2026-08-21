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
using sim::lpc_memory::firmware_ahb_layout;
using sim::lpc_memory::firmware_main_sram_layout;
using sim::lpc_memory::MainSramLayout;
using sim::lpc_memory::MemoryAccounting;
using sim::lpc_memory::MemoryRegion;
using sim::lpc_memory::UnifiedHeapModel;
using sim::test::require;

void generated_layout_matches_the_pinned_arm_firmware() {
  const auto main = firmware_main_sram_layout();
  require(main.ram_start == 0x100000c8, "main RAM origin should come from the ARM linker map");
  require(main.ram_end == 0x10008000, "main RAM end should come from the ARM linker map");
  require(main.static_end == 0x10004d88, "static RAM end should come from the linked image");
  require(main.heap_start == 0x10004d88, "main heap start should come from the linker map");
  require(main.heap_end == 0x10006fe0, "main heap end should preserve the MPU guard");

  const auto ahb = firmware_ahb_layout();
  require(ahb.region_start == 0x2007c000, "AHB RAM origin should come from the ARM linker map");
  require(ahb.region_end == 0x20084000, "AHB RAM end should come from the ARM linker map");
  require(ahb.static_end == 0x2007c400, "static AHB end should follow linked reserved data");
  require(ahb.heap_start == 0x2007c400, "AHB heap start should come from the linker map");
  require(ahb.heap_end - ahb.heap_start == 31'744, "AHB heap span should match the pinned image");
}

void unified_heap_spills_allocations_from_main_sram_into_ahb_sram() {
  UnifiedHeapModel memory(MainSramLayout{
      .ram_start = 0x10000000,
      .ram_end = 0x10000080,
      .static_end = 0x10000000,
      .stack_top = 0x10000080,
      .stack_limit = 0x10000070,
      .heap_start = 0x10000000,
      .heap_end = 0x10000040,
  }, AhbLayout{
      .region_start = 0x2007c000,
      .region_end = 0x2007c080,
      .static_end = 0x2007c040,
      .heap_start = 0x2007c040,
      .heap_end = 0x2007c080,
  });

  require(memory.allocate(1, 40) == MemoryRegion::MainSram, "the first suitable main-SRAM block should win");
  require(memory.allocate(2, 8) == MemoryRegion::AhbSram,
          "allocation should spill into AHB SRAM after exhausting the main region");
  auto snapshot = memory.snapshot();
  require(snapshot.heap.capacity_bytes == 112, "each heap_5 region should reserve one eight-byte sentinel");
  require(snapshot.main.live_payload_bytes == 40, "main SRAM should report its resident allocation");
  require(snapshot.ahb.live_payload_bytes == 8, "AHB SRAM should report its resident allocation");
  require(snapshot.heap.total_free_bytes == 40, "unified free bytes should sum both physical regions");
  require(snapshot.heap.largest_free_block_bytes == 40, "largest free block should span either region");

  require(!memory.allocate(3, 64), "allocation larger than every free block should fail");
  snapshot = memory.snapshot();
  require(snapshot.heap.failed_allocation_count == 1, "unified heap exhaustion should be retained in the report");

  memory.deallocate(1);
  memory.deallocate(2);
  snapshot = memory.snapshot();
  require(snapshot.heap.total_free_bytes == 112, "freeing allocations should restore both heap regions");
  require(snapshot.heap.minimum_ever_free_bytes == 40, "minimum-ever-free should survive later frees");
}

void accounting_service_assigns_groups_to_the_physical_region_used_by_the_unified_heap() {
  MemoryAccounting memory;
  int pointer = 0;

  memory.record_heap(&pointer, 16, 8, "TargetType");

  auto snapshot = memory.snapshot();
  require(snapshot.main.live_payload_bytes == 8, "main heap should use the target payload size");
  require(snapshot.allocation_groups.size() == 1, "allocation should be grouped for reporting");
  require(snapshot.allocation_groups[0].region == MemoryRegion::MainSram,
          "allocation group should identify the physical heap region selected by heap_5");
  require(snapshot.allocation_groups[0].host_payload_bytes == 16, "report should retain the host request");
  require(snapshot.allocation_groups[0].target_payload_bytes == 8, "report should retain the LPC charge");

  require(memory.deallocate(&pointer) == MemoryRegion::MainSram, "tracked heap allocation should be released");
  require(memory.snapshot().main.live_payload_bytes == 0, "released payload should no longer be live");
}

void generic_allocations_are_only_charged_when_the_lpc_layout_is_known() {
  const auto cartesian =
      sim::lpc_memory::resolve_generic_heap_allocation(sizeof(CartesianSolution), false, "Robot::load_config()+0x40");
  require(cartesian.target_size_exact, "known firmware object allocation should have an exact LPC charge");
  require(cartesian.type_name == "CartesianSolution", "known allocation should report its firmware type");

  const auto command_buffer =
      sim::lpc_memory::resolve_generic_heap_allocation(128, true, "ZProbe::coordinated_move()+0x20");
  require(command_buffer.target_size_exact, "known byte-array allocation should preserve its exact byte count");
  require(command_buffer.target_payload_bytes == 128, "byte arrays should occupy the requested bytes on the LPC");
  require(command_buffer.type_name == "char[]", "known byte-array allocation should report its element type");

  const auto reserved_float_storage = sim::lpc_memory::resolve_generic_heap_allocation(
      4096, false, "Endstops::test_endstop_repeatability(Gcode*)",
      "float* std::__1::__libcpp_allocate<float>(std::__1::__element_count, unsigned long)");
  require(reserved_float_storage.target_size_exact,
          "explicitly reserved float-vector storage should have an exact LPC charge");
  require(reserved_float_storage.target_payload_bytes == 4096,
          "float-vector storage should occupy the same bytes on host and LPC");
  require(
      reserved_float_storage.type_name == "std::vector<float> storage @ Endstops::test_endstop_repeatability(Gcode*)",
      "float-vector storage should report the owning firmware function");

  const auto unresolved = sim::lpc_memory::resolve_generic_heap_allocation(37, false, "UnknownFirmwareFunction()+0x10");
  require(!unresolved.target_size_exact, "unknown ABI-dependent allocations must remain outside LPC totals");
  require(unresolved.type_name == "ABI-unresolved @ UnknownFirmwareFunction()+0x10",
          "unknown allocations should still identify their firmware origin");
}

}  // namespace

int main() {
  generated_layout_matches_the_pinned_arm_firmware();
  unified_heap_spills_allocations_from_main_sram_into_ahb_sram();
  accounting_service_assigns_groups_to_the_physical_region_used_by_the_unified_heap();
  generic_allocations_are_only_charged_when_the_lpc_layout_is_known();
  return 0;
}
