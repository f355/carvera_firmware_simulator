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
#include <filesystem>
#include <iostream>
#include <string>

#include "Conveyor.h"
#include "Robot.h"
#include "StepTicker.h"
#include "libs/Kernel.h"
#include "modules/communication/SerialConsole.h"
#include "sim/machine_simulator.hpp"
#include "sim/motion_runner.hpp"
#include "sim/persistent_machine_state.hpp"
#include "support/temp_sdcard.hpp"
#include "support/direct_robot_config.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void run_main_loop_until_serial_drained(Kernel& kernel, int max_iterations) {
  for (int i = 0; i < max_iterations && kernel.serial->has_char('\n'); ++i) {
    kernel.call_event(ON_MAIN_LOOP);
  }
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_serial_console_test");
  const auto& root = temp_root.path();
  sim::test::write_direct_robot_config(root);
  sim::PersistentMachineState persistent_state(sim::test::persistent_sd_config(root));
  persistent_state.eeprom().reset();
  persistent_state.eeprom().configure_factory_settings({sim::MachineModel::CarveraC1, 0x04});

  sim::MachineSimulator simulator(persistent_state);
  Kernel kernel;

  const auto axis = simulator.add_step_dir_axis({1, 18}, {1, 20});
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();

  kernel.serial->serial->simulate_rx("G91\nG0 X5 F1500\n");
  run_main_loop_until_serial_drained(kernel, 8);

  sim::MotionRunner runner(simulator, kernel);
  require(runner.run_until_idle(50'000), "serial-fed G-code motion should execute to idle");
  require(simulator.axis_position_steps(axis) == 50,
          "physical axis position should reflect serial-fed G-code step/dir pulses");
  require(kernel.robot->get_axis_position(0) == 5.0F, "Robot should update position from serial-fed G-code jog");
  require(kernel.serial->serial->take_tx().find("ok") != std::string::npos,
          "SerialConsole should write G-code acknowledgements to the UART");
  return 0;
}
