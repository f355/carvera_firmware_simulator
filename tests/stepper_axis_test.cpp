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

#include "LPC17xx.h"
#include "Pin.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/stepper_axis.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void pulse_step() {
  LPC_GPIO1->FIOSET = 1u << 18;
  LPC_GPIO1->FIOCLR = 1u << 18;
}

void configure_step_dir_outputs() { LPC_GPIO1->FIODIR |= (1u << 18) | (1u << 20); }

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

  motor.set_direction(false);
  motor.step();
  require(simulator.axis_position_steps(axis) == 1,
          "physical axis should advance on a step rising edge with direction low");
  motor.unstep();
  require(simulator.axis_position_steps(axis) == 1, "physical axis should not move on the falling edge");

  motor.set_direction(true);
  motor.step();
  require(simulator.axis_position_steps(axis) == 0,
          "physical axis should reverse on a step rising edge with direction high");
  motor.unstep();

  simulator.reset();
  configure_step_dir_outputs();

  sim::StepperAxisConfig dual_endstop_axis;
  dual_endstop_axis.step_pin = {1, 18};
  dual_endstop_axis.direction_pin = {1, 20};
  dual_endstop_axis.steps_per_mm = 10.0;
  dual_endstop_axis.endstops.push_back(
      sim::StepperEndstopConfig{sim::EndstopSide::Min, true, {0, 1}, -1.0, false, true});
  dual_endstop_axis.endstops.push_back(sim::StepperEndstopConfig{sim::EndstopSide::Max, true, {0, 2}, 1.0, true, true});
  const auto dual_axis = sim::stepper_axes::add_axis(dual_endstop_axis);

  require(!simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Min),
          "min endstop should be open at the center of travel");
  require(!simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Max),
          "max endstop should be open at the center of travel");
  require(!simulator.gpio_level({0, 1}), "min endstop GPIO should be low before the min switch");
  require(!simulator.gpio_level({0, 2}), "max endstop GPIO should be low before the max switch");

  for (int i = 0; i < 10; ++i) {
    pulse_step();
  }
  require(simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Max),
          "max endstop should trigger at positive travel");
  require(!simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Min),
          "min endstop should stay open at positive travel");
  require(simulator.gpio_level({0, 2}), "max endstop GPIO should be driven high at positive travel");

  LPC_GPIO1->FIOSET = 1u << 20;
  for (int i = 0; i < 20; ++i) {
    pulse_step();
  }
  require(simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Min),
          "min endstop should trigger at negative travel");
  require(!simulator.axis_endstop_triggered(dual_axis, sim::EndstopSide::Max),
          "max endstop should release at negative travel");
  require(simulator.gpio_level({0, 1}), "min endstop GPIO should be driven high at negative travel");
  require(!simulator.gpio_level({0, 2}), "max endstop GPIO should be low after leaving positive travel");

  simulator.reset();
  configure_step_dir_outputs();

  sim::StepperAxisConfig shared_switch_axis;
  shared_switch_axis.step_pin = {1, 18};
  shared_switch_axis.direction_pin = {1, 20};
  shared_switch_axis.steps_per_mm = 10.0;
  shared_switch_axis.endstops.push_back(
      sim::StepperEndstopConfig{sim::EndstopSide::Min, true, {0, 3}, -1.0, false, true});
  shared_switch_axis.endstops.push_back(
      sim::StepperEndstopConfig{sim::EndstopSide::Max, true, {0, 3}, 1.0, true, true});
  const auto shared_axis = sim::stepper_axes::add_axis(shared_switch_axis);

  require(!simulator.gpio_level({0, 3}), "shared endstop GPIO should be low at the center");
  for (int i = 0; i < 10; ++i) {
    pulse_step();
  }
  require(simulator.axis_endstop_triggered(shared_axis, sim::EndstopSide::Max),
          "shared max side should trigger at positive travel");
  require(simulator.gpio_level({0, 3}), "shared endstop GPIO should be high at positive travel");
  LPC_GPIO1->FIOSET = 1u << 20;
  for (int i = 0; i < 10; ++i) {
    pulse_step();
  }
  require(!simulator.gpio_level({0, 3}), "shared endstop GPIO should release back inside travel");
  for (int i = 0; i < 10; ++i) {
    pulse_step();
  }
  require(simulator.axis_endstop_triggered(shared_axis, sim::EndstopSide::Min),
          "shared min side should trigger at negative travel");
  require(simulator.gpio_level({0, 3}), "shared endstop GPIO should be high at negative travel");

  return 0;
}
