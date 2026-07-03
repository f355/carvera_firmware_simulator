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

#include "test_support.hpp"

#include "Config.h"
#include "Conveyor.h"
#include "Gcode.h"
#include "Robot.h"
#include "StepTicker.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/event_engine.hpp"
#include "sim/persistent_machine_state.hpp"
#include "support/direct_robot_config.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::PersistentMachineState persistent_state;
  persistent_state.eeprom().reset();
  persistent_state.eeprom().configure_factory_settings({sim::MachineModel::CarveraC1, 0x04});

  sim::MachineSimulator simulator(persistent_state);
  Kernel kernel;

  const auto x_axis = simulator.add_step_dir_axis({1, 18}, {1, 20});
  const auto y_axis = simulator.add_step_dir_axis({1, 19}, {1, 21});

  kernel.config = new Config(new MemoryConfigSource(sim::test::direct_robot_config_lines()));
  kernel.config->config_cache_load();
  kernel.robot->on_module_loaded();

  require(kernel.robot->get_number_registered_motors() == 5, "Robot should load stock-like XYZAB motors from config");
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();

  Gcode relative_mode("G91", kernel.streams, true, 1);
  kernel.robot->on_gcode_received(&relative_mode);

  Gcode jog("G0 X5 F1500", kernel.streams, true, 2);
  kernel.robot->on_gcode_received(&jog);
  require(!jog.is_error, "Robot should accept a minimal G0 jog command");

  sim::EventEngine engine(simulator);
  require(engine.run_until_motion_idle(kernel, 50'000).status == sim::EventRunStatus::ConditionReached,
          "simulator should execute G-code Robot motion to idle");
  require(simulator.axis_position_steps(x_axis) == 50,
          "physical axis position should reflect G-code-generated step/dir pulses");
  require(kernel.robot->get_axis_position(0) == 5.0F, "Robot should update position from G-code jog");

  Gcode arc("G2 X0.2 Y0 I0.1 J0 F600", kernel.streams, true, 3);
  kernel.robot->on_gcode_received(&arc);
  require(!arc.is_error, "Robot should accept a clockwise arc command");

  require(engine.run_until_motion_idle(kernel, 100'000).status == sim::EventRunStatus::ConditionReached,
          "simulator should execute G-code Robot arc motion to idle");
  require(simulator.axis_position_steps(x_axis) == 52,
          "physical X axis should reflect G2 arc endpoint step/dir pulses");
  require(simulator.axis_position_steps(y_axis) == 0, "physical Y axis should return to the G2 arc endpoint");
  require(kernel.robot->get_axis_position(0) == 5.2F, "Robot should update X after G2 arc");
  require(kernel.robot->get_axis_position(1) == 0.0F, "Robot should update Y after G2 arc");

  Gcode absolute_mode("G90", kernel.streams, true, 4);
  kernel.robot->on_gcode_received(&absolute_mode);

  Gcode rotate_wcs("G10 L2 P1 R90", kernel.streams, true, 5);
  kernel.robot->on_gcode_received(&rotate_wcs);
  require(!rotate_wcs.is_error, "Robot should accept WCS rotation on G10 L2");

  Gcode rotated_move("G0 X1 Y0 F1500", kernel.streams, true, 6);
  kernel.robot->on_gcode_received(&rotated_move);
  require(!rotated_move.is_error, "Robot should accept a move through a rotated WCS");

  require(engine.run_until_motion_idle(kernel, 100'000).status == sim::EventRunStatus::ConditionReached,
          "simulator should execute rotated WCS motion to idle");
  require(simulator.axis_position_steps(x_axis) == 0,
          "rotated WCS X target should move the machine X axis to zero at R90");
  require(simulator.axis_position_steps(y_axis) == 10, "rotated WCS X target should move the machine Y axis at R90");

  return 0;
}
