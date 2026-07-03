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

#include <cstdint>

#define private public
#define protected public
#include "Conveyor.h"
#include "Planner.h"
#undef protected
#undef private

#include "Pin.h"
#include "Robot.h"
#include "StepTicker.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/event_engine.hpp"
#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

void queue_one_axis_move(Kernel& kernel) {
  ActuatorCoordinates target{};
  target[0] = 1.0F;
  float unit_vec[N_PRIMARY_AXIS] = {1.0F, 0.0F, 0.0F};

  require(kernel.planner->append_block(target, 1, 25.0F, 1.0F, unit_vec, 100.0F, 0.0F, true, 1),
          "Planner should accept a one-axis move");
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
  require(kernel.robot->register_motor(&motor) == 0, "Robot should register the first motor");
  kernel.conveyor->start(1);
  queue_one_axis_move(kernel);

  sim::EventEngine engine(simulator);
  const auto stalled = engine.run_until_motion_idle(kernel, 1000);
  require(stalled.status == sim::EventRunStatus::NoProgress,
          "event engine should report no progress before timer IRQs are enabled");
  require(simulator.axis_position_steps(axis) == 0, "physical axis should not move while Timer0 IRQ is disabled");

  kernel.step_ticker->start();

  const auto limited = engine.run_until_motion_idle(kernel, 1);
  require(limited.status == sim::EventRunStatus::BudgetExhausted,
          "event engine should distinguish an exhausted budget from no progress");

  const auto completed = engine.run_until_motion_idle(kernel, 50'000);
  require(completed.status == sim::EventRunStatus::ConditionReached,
          "event engine should reach motion idle after StepTicker starts IRQs");
  require(simulator.axis_position_steps(axis) == 10,
          "physical axis should move from Timer0/Timer1-dispatched step pulses");

  return 0;
}
