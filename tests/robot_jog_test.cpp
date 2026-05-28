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

#include <cstddef>
#include <cstdlib>
#include <iostream>

#include "Conveyor.h"
#include "Pin.h"
#include "Robot.h"
#include "StepTicker.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/motion_runner.hpp"

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
  Kernel kernel;

  const auto axis = simulator.add_step_dir_axis({1, 18}, {1, 20});

  Pin step_pin;
  Pin dir_pin;
  Pin enable_pin;
  require(step_pin.from_string("1.18") != nullptr, "step pin should parse");
  require(dir_pin.from_string("1.20") != nullptr, "direction pin should parse");
  step_pin.as_output();
  dir_pin.as_output();

  StepperMotor motor(step_pin, dir_pin, enable_pin);
  motor.change_steps_per_mm(10.0F);
  motor.set_max_rate(100.0F);
  motor.set_acceleration(1000.0F);

  require(kernel.robot->register_motor(&motor) == 0, "Robot should register the first motor at index zero");
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();

  const float delta[1] = {5.0F};
  require(kernel.robot->delta_move(delta, 25.0F, 1), "Robot should queue a jog-like delta move");

  sim::MotionRunner runner(kernel);
  require(runner.run_until_idle(50'000), "simulator should execute Robot-queued motion to idle");
  require(simulator.axis_position_steps(axis) == 50,
          "physical axis position should reflect Robot-generated step/dir pulses");
  require(kernel.robot->get_axis_position(0) == 5.0F, "Robot should update the machine position");

  return 0;
}
