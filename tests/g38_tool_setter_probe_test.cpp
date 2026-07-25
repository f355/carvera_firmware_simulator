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
#include "support/assertions.hpp"
#include "support/probe_runtime.hpp"
#include "support/runtime_wait.hpp"

int main() {
  using sim::test::require;
  using sim::test::require_near;

  sim::test::ProbeRuntime probe("carvera_sim_g38_tool_setter_probe_test");
  auto& simulator = probe.simulator();
  auto& runtime = probe.runtime();
  auto& kernel = probe.kernel();

  require(runtime.is_homed(), "runtime should home before tool setter probing");
  const double current_x = simulator.axis_position_mm(0);
  const double current_y = simulator.axis_position_mm(1);
  const double target_z = simulator.axis_position_mm(2) - 1.0;
  probe.world().set_tool_setter_box(
      sim::Box{current_x - 5.0, current_y - 5.0, target_z - 0.25, current_x + 5.0, current_y + 5.0, target_z});

  runtime.io().write_serial_command("G91\nG38.6 Z-10 F60\n");
  sim::test::require_motion_idle(runtime.runner(), 200'000, "G38.6 tool setter move should stop and reach idle");
  const auto serial = runtime.io().read_serial_text();
  require(serial.find("[PRB:") != std::string::npos, "G38.6 should report a calibrated position");
  require(serial.find(":1]") != std::string::npos, "G38.6 should report tool setter success");
  require_near(simulator.axis_position_mm(2), target_z, 0.08,
               "physical Z position should stop near the tool setter top face");
  require_near(kernel.robot->get_axis_position(2), target_z - 1.0, 0.08,
               "firmware Z coordinate should reflect the switch-to-soft-limit physical offset");

  return 0;
}
