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

#include "Robot.h"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/probe_runtime.hpp"
#include "sim/simulator_context.hpp"

int main() {
  using sim::test::require;
  using sim::test::require_near;

  sim::test::ProbeRuntime probe("carvera_sim_g38_3d_probe_side_test");
  auto& simulator = probe.simulator();
  auto& runtime = probe.runtime();
  auto& kernel = probe.kernel();

  require(runtime.is_homed(), "runtime should home before side probing");
  simulator.context().physical_scene().set_spindle_tool(999999, 50.0, true, sim::ToolKind::ThreeAxisProbe, 2.0);

  const double current_x = simulator.axis_position_mm(0);
  const double current_y = simulator.axis_position_mm(1);
  const double spindle_face_z = simulator.axis_position_mm(2);
  const double tip_z = spindle_face_z - 30.0;
  const double stock_min_x = current_x + 4.0;
  probe.world().set_stock_box(
      sim::Box{stock_min_x, current_y - 5.0, tip_z - 5.0, stock_min_x + 10.0, current_y + 5.0, tip_z + 5.0});

  runtime.io().write_serial("G91\nG38.2 X10 F60\n");
  require(runtime.runner().run_until_motion_idle(250'000).motion_idle,
          "G38.2 side probe move should stop and reach idle");
  const auto serial = runtime.io().read_serial();
  require(serial.find("[PRB:") != std::string::npos, "G38.2 should report a probed position");
  require(serial.find(":1]") != std::string::npos, "G38.2 should report probe success");
  require_near(simulator.axis_position_mm(0), stock_min_x - 1.0, 0.08,
               "3-axis probe ball should stop one radius before the stock side face");
  require_near(kernel.robot->get_axis_position(0), stock_min_x - 2.0, 0.08,
               "firmware X coordinate should reflect the switch-to-soft-limit physical offset");

  return 0;
}
