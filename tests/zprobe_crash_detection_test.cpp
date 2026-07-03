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

#include <string>

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/probe_runtime.hpp"
#include "support/runtime_wait.hpp"
#include "sim/simulator_context.hpp"

int main() {
  using sim::test::require;
  using sim::test::require_near;

  sim::test::ProbeRuntime probe("carvera_sim_zprobe_crash_detection_test");
  auto& simulator = probe.simulator();
  auto& runtime = probe.runtime();
  auto& kernel = probe.kernel();

  require(runtime.is_homed(), "runtime should home before crash-detection motion");
  runtime.write_serial("M493.2 T999999\n");
  require(runtime.run_until_idle(100'000), "direct firmware tool-state command should run");
  (void)runtime.read_serial();
  simulator.context().physical_scene().set_spindle_tool(999999, 50.0, true, sim::ToolKind::ThreeAxisProbe, 2.0);
  runtime.run_main_loop(4);

  const double current_x = simulator.axis_position_mm(0);
  const double current_y = simulator.axis_position_mm(1);
  const double spindle_face_z = simulator.axis_position_mm(2);
  const double tip_z = spindle_face_z - 30.0;
  const double stock_min_x = current_x + 4.0;
  runtime.set_stock_box(
      sim::Box{stock_min_x, current_y - 5.0, tip_z - 5.0, stock_min_x + 10.0, current_y + 5.0, tip_z + 5.0});

  runtime.write_serial("G91\nG0 X10 F60\n");
  std::string serial;
  const bool halted = sim::test::pump_until(runtime, [&] {
    serial += runtime.read_serial();
    return kernel.is_halted();
  });
  serial += runtime.read_serial();

  require(halted, "real ZProbe should halt firmware when a 3D probe contacts stock during normal motion");
  require(kernel.get_halt_reason() == CRASH_DETECTED,
          "3D probe contact outside a probe cycle should report CRASH_DETECTED");
  require(serial.find("3D Probe crash detected") != std::string::npos,
          "crash detection should explain the probe crash on the firmware stream");
  require_near(simulator.axis_position_mm(0), stock_min_x - 1.0, 0.12,
               "3D probe crash should occur when the probe ball first touches the stock side");
  require_near(kernel.robot->get_axis_position(0), stock_min_x - 2.0, 0.12,
               "firmware X coordinate should reflect the switch-to-soft-limit physical offset");

  return 0;
}
