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

#include <cstdlib>
#include <iostream>

#include "sim/machine_simulator.hpp"
#include "sim/physical_scene.hpp"
#include "sim/simulator_context.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  auto& scene = simulator.context().physical_scene();

  scene.set_probe_tool_installed(false);
  scene.set_stock_box(sim::Box{
      -5.0,
      -5.0,
      -2.0,
      5.0,
      5.0,
      -1.0,
  });
  scene.update_probe_contacts({0.0, 0.0, -1.5});
  require(!simulator.gpio_level({2, 6}), "stock should not trigger the spindle probe unless a probe tool is installed");

  scene.set_probe_tool_installed(true);
  scene.update_probe_contacts({0.0, 0.0, -1.0});
  require(simulator.gpio_level({2, 6}), "stock Z probe should trigger on the stock top face");

  scene.update_probe_contacts({-6.0, 0.0, -1.5});
  scene.update_probe_contacts({0.0, 0.0, -1.5});
  require(!simulator.gpio_level({2, 6}), "stock Z probe side contact should not trigger the probe input");
  require(scene.stock_probe_crash_axis().has_value() && *scene.stock_probe_crash_axis() == 0,
          "stock Z probe side contact should report an X-axis crash");

  scene.update_probe_contacts({7.0, 0.0, -1.5});
  require(!simulator.gpio_level({2, 6}), "stock box should release the spindle probe outside its bounds");

  scene.set_tool_setter_box(sim::Box{
      100.0,
      100.0,
      -1.0,
      110.0,
      110.0,
      1.0,
  });
  scene.update_probe_contacts({105.0, 105.0, 0.0});
  require(simulator.gpio_level({0, 5}), "tool setter box should trigger the tool-setter input");

  scene.set_spindle_tool(0, 50.0, true, sim::ToolKind::StockZProbe, 1.6);
  scene.update_probe_contacts({105.0, 105.0, 31.0});
  require(simulator.gpio_level({2, 6}), "probe tool touching the tool setter should trigger the spindle probe input");
  require(simulator.gpio_level({0, 5}), "probe tool touching the tool setter should keep the tool-setter input active");

  scene.set_spindle_tool(999990, 50.0, true, sim::ToolKind::ThreeAxisProbe, 2.0);
  scene.update_probe_contacts({105.0, 105.0, 21.0});
  require(simulator.gpio_level({2, 6}), "firmware high-numbered probe tools should trigger the spindle probe input");
  require(simulator.gpio_level({0, 5}), "firmware high-numbered probe tools should keep the tool-setter input active");

  scene.set_spindle_tool(1, 50.0, true);
  scene.update_probe_contacts({105.0, 105.0, 21.0});
  require(!simulator.gpio_level({2, 6}),
          "non-probe tools should not trigger the spindle probe input at the tool setter");
  require(simulator.gpio_level({0, 5}), "non-probe tool tips should still trigger the tool-setter input");

  scene.clear();
  scene.set_atc_pocket_tool(1, 1, true, 52.0);
  scene.update_atc_clamp_position({0.0, 0.0, -5.0}, 0.0);
  scene.update_atc_clamp_position({0.0, 0.0, -5.0}, 1.0);
  require(!scene.atc_spindle().has_tool, "ATC clamp should not pick a rack tool above the pocket Z");
  scene.update_atc_clamp_position({0.0, 0.0, 0.0}, 0.0);
  require(!scene.atc_spindle().has_tool, "ATC opening travel should not pick a rack tool");
  scene.update_atc_clamp_position({0.0, 0.0, 0.0}, 1.0);
  require(scene.atc_spindle().has_tool && scene.atc_spindle().tool == 1,
          "ATC closing travel at rack height should pick the pocket tool");
  scene.update_atc_clamp_position({0.0, 0.0, 0.0}, 0.0);
  require(!scene.atc_spindle().has_tool, "ATC opening travel at rack height should drop the spindle tool");

  scene.set_stock_box(sim::Box{
      10.0,
      100.0,
      -10.0,
      20.0,
      110.0,
      0.0,
  });
  scene.set_spindle_tool(999999, 50.0, true, sim::ToolKind::ThreeAxisProbe, 2.0);
  scene.update_probe_contacts({9.25, 105.0, 25.0});
  require(simulator.gpio_level({2, 6}), "3-axis probe ball should trigger on stock side contact");

  scene.set_spindle_tool(0, 50.0, true, sim::ToolKind::StockZProbe, 1.6);
  scene.update_probe_contacts({9.25, 105.0, 25.0});
  scene.update_probe_contacts({10.25, 105.0, 25.0});
  require(!simulator.gpio_level({2, 6}), "stock Z probe should not trigger on stock side contact");
  require(scene.stock_probe_crash_axis().has_value() && *scene.stock_probe_crash_axis() == 0,
          "stock Z probe side contact should report a physical X crash");

  scene.clear();
  require(!simulator.gpio_level({2, 6}), "clearing scene should release probe input");
  require(!simulator.gpio_level({0, 5}), "clearing scene should release tool-setter input");

  return 0;
}
