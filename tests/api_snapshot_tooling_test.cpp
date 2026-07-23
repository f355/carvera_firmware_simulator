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

#include "sim/physical_scene.hpp"
#include "support/api_service_harness.hpp"
#include "support/api_snapshot_config.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"
#include "sim/simulator_context.hpp"

int main() {
  using sim::test::require;

  sim::test::TempSdCard sd("carvera_sim_api_snapshot_test");
  sim::test::write_api_snapshot_config(sd.path());
  sim::test::ApiHarness api(sd.persistent_config());
  auto response = api.request([](auto& request) {
    auto* atc_tools = request.mutable_set_atc_pocket_tools();
    atc_tools->set_replace(true);
    auto* atc_tool = atc_tools->add_tools();
    atc_tool->set_pocket(1);
    atc_tool->set_tool(1);
    atc_tool->set_occupied(true);
    atc_tool->set_length_mm(62.0);
    atc_tool->set_kind(sim::test::pb::TOOL_KIND_CUTTING_TOOL);
  });
  require(response.ok(), "set_atc_pocket_tools should accept rack contents before boot");

  response = api.request([](auto& request) {
    auto* spindle_tool = request.mutable_set_spindle_tool();
    spindle_tool->set_installed(true);
    spindle_tool->set_tool(3);
    spindle_tool->set_length_mm(55.5);
    spindle_tool->set_kind(sim::test::pb::TOOL_KIND_THREE_AXIS_PROBE);
    spindle_tool->set_probe_tip_diameter_mm(2.5);
  });
  require(response.ok(), "set_spindle_tool should accept a manual spindle tool");
  const auto spindle = api.simulator().context().physical_scene().atc_spindle();
  require(spindle.has_tool, "set_spindle_tool should load the simulated spindle");
  require(spindle.tool == 3, "set_spindle_tool should store the tool number");
  require(spindle.length_mm == 55.5, "set_spindle_tool should store the tool length");
  require(spindle.kind == sim::ToolKind::ThreeAxisProbe, "set_spindle_tool should store the physical tool kind");
  require(spindle.probe_tip_diameter_mm == 2.5, "set_spindle_tool should store the probe tip diameter");

  response = api.request([](auto& request) { request.mutable_get_machine_snapshot(); });
  require(response.ok(), "get_machine_snapshot should boot firmware and succeed");
  require(response.machine_snapshot().homed(), "machine snapshot should report boot homing complete");
  require(response.machine_snapshot().soft_endstop_enabled(),
          "machine snapshot should report soft-endstop enable state");
  require(response.machine_snapshot().work_area().min_x() == -10.0,
          "machine snapshot should expose configured soft-limit X min");
  require(response.machine_snapshot().work_area().max_x() == -1.0,
          "machine snapshot should expose firmware soft-limit X max");
  require(response.machine_snapshot().physical_travel().min_x() == -372.0,
          "C1 machine snapshot should expose simulator-owned physical X travel");
  require(response.machine_snapshot().physical_travel().max_z() == 1.0,
          "C1 machine snapshot should expose simulator-owned physical Z travel");
  require(response.machine_snapshot().tool_setter_available(),
          "machine snapshot should expose configured physical tool-setter geometry");
  require(response.machine_snapshot().tool_setter().max_z() > response.machine_snapshot().tool_setter().min_z(),
          "machine snapshot tool-setter geometry should have height");
  require(response.machine_snapshot().axes_size() >= 5, "machine snapshot should include configured A and ATC axes");
  require(response.machine_snapshot().axes(0).axis() == sim::test::pb::AXIS_X,
          "machine snapshot axis zero should be X");
  require(response.machine_snapshot().axes(3).axis() == sim::test::pb::AXIS_A,
          "machine snapshot axis three should be the configured rotary/A actuator");
  require(response.machine_snapshot().axes(4).axis() == sim::test::pb::AXIS_B,
          "machine snapshot axis four should be the configured ATC/B actuator");
  require(response.machine_snapshot().axes(1).physical_mm() < -3.9,
          "machine snapshot should expose physical Y backed off from the stock hard switch");
  require(response.machine_snapshot().atc().pockets_size() == 1, "machine snapshot should include configured ATC tool");
  require(response.machine_snapshot().atc().pockets(0).length_mm() == 62.0,
          "machine snapshot should report configured ATC tool length");
  require(response.machine_snapshot().atc().spindle().kind() == sim::test::pb::TOOL_KIND_THREE_AXIS_PROBE,
          "machine snapshot should expose simulated spindle tool kind");
  require(response.machine_snapshot().atc().spindle().probe_tip_diameter_mm() == 2.5,
          "machine snapshot should expose simulated spindle probe tip diameter");
  require(response.machine_snapshot().has_memory(), "periodic machine snapshots should include LPC memory totals");
  require(response.machine_snapshot().memory().main().capacity_bytes() == 32'568,
          "memory summary should expose the LPC1768 main SRAM capacity");
  require(response.machine_snapshot().memory().main().live_payload_bytes() > 0,
          "memory summary should expose live target heap allocations");
  require(response.machine_snapshot().memory().ahb().live_payload_bytes() > 0,
          "memory summary should expose live target AHB allocations");

  response = api.request([](auto& request) { request.mutable_get_memory_details(); });
  require(response.ok(), "get_memory_details should succeed after firmware boot");
  require(response.has_memory_details(), "get_memory_details should return a structured report");
  bool saw_robot = false;
  bool saw_blocks = false;
  for (const auto& group : response.memory_details().allocation_groups()) {
    saw_robot = saw_robot || (group.region() == sim::test::pb::MEMORY_REGION_MAIN_SRAM &&
                              group.type_name() == "Robot" && group.target_size_exact());
    saw_blocks = saw_blocks || (group.region() == sim::test::pb::MEMORY_REGION_AHB_SRAM &&
                                group.type_name() == "Block[]" && group.target_size_exact());
  }
  require(saw_robot, "memory details should identify the Robot allocation with its target ABI size");
  require(saw_blocks, "memory details should identify the AHB motion block queue");

  response = api.request([](auto& request) { request.mutable_get_laser_state(); });
  require(response.ok(), "get_laser_state should succeed");
  require(response.laser_state().available(), "get_laser_state should expose real Laser PublicData");
  require(!response.laser_state().mode(), "get_laser_state should start in CNC mode");
  require(!response.laser_state().firing(), "get_laser_state should start with laser firing off");
  require(response.laser_state().scale_percent() == 100.0, "get_laser_state should expose laser scale");

  return 0;
}
