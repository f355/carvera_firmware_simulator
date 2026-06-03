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

#include "libs/Kernel.h"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/probe_runtime.hpp"
#include "support/runtime_wait.hpp"

int main() {
  using sim::test::require;

  sim::test::ProbeRuntime probe("carvera_sim_stock_z_probe_side_crash_test", "alpha_motor_alarm_pin 0.1!^\n");
  auto& simulator = probe.simulator();
  auto& runtime = probe.runtime();
  auto& kernel = probe.kernel();

  require(runtime.is_homed(), "runtime should home before stock Z probe crash test");
  sim::physical_scene::active().set_spindle_tool(0, 50.0, true, sim::ToolKind::StockZProbe, 1.6);
  runtime.run_main_loop(4);

  const double current_x = simulator.axis_position_mm(0);
  const double current_y = simulator.axis_position_mm(1);
  const double tip_z = simulator.axis_position_mm(2) - 30.0;
  runtime.set_stock_box(
      sim::Box{current_x - 14.0, current_y - 5.0, tip_z - 5.0, current_x - 4.0, current_y + 5.0, tip_z + 5.0});

  runtime.write_serial("G91\nG0 X-10 F60\n");
  std::string serial;
  const bool halted = sim::test::pump_until(runtime, [&] {
    serial += runtime.read_serial();
    return kernel.is_halted();
  });
  serial += runtime.read_serial();

  require(halted, "stock Z probe side contact should halt through a simulated motor alarm");
  require(kernel.get_halt_reason() == MOTOR_ERROR_X, "stock Z probe side contact in X should report MOTOR_ERROR_X");
  require(runtime.motor_alarm(0), "stock Z probe side contact should drive the X motor alarm input");
  require(serial.find("3D Probe crash detected") == std::string::npos,
          "stock Z probe side contact should not masquerade as a 3-axis probe signal");
  require(!runtime.probe_inputs().first, "stock Z probe side contact should not assert the probe input");

  return 0;
}
