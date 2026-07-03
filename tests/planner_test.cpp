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
#include <cstdint>

#define private public
#define protected public
#include "Conveyor.h"
#include "Planner.h"
#undef protected
#undef private

#include "Block.h"
#include "Pin.h"
#include "Planner.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"

#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

using sim::test::require;


int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  require(kernel.planner != nullptr, "Kernel should create a Planner");
  require(kernel.planner->max_allowable_speed(-100.0F, 0.0F, 2.0F) > 19.9F,
          "Planner should expose real max allowable speed math");

  Pin step_pin;
  Pin dir_pin;
  Pin enable_pin;
  require(step_pin.from_string("1.18") != nullptr, "step pin should parse");
  require(dir_pin.from_string("1.20") != nullptr, "direction pin should parse");
  step_pin.as_output();
  dir_pin.as_output();

  StepperMotor motor(step_pin, dir_pin, enable_pin);
  motor.change_steps_per_mm(10.0F);
  require(kernel.robot->register_motor(&motor) == 0, "Robot should register the first motor at index zero");
  kernel.conveyor->start(1);

  ActuatorCoordinates target{};
  target[0] = 5.0F;
  float unit_vec[N_PRIMARY_AXIS] = {1.0F, 0.0F, 0.0F};

  require(kernel.planner->append_block(target, 1, 25.0F, 5.0F, unit_vec, 100.0F, 0.0F, true, 42),
          "Planner should append a one-axis movement block");
  require(!kernel.conveyor->is_queue_empty(), "Planner should queue the prepared block");

  Block* block = kernel.conveyor->queue.tail_ref();
  require(block->line == 42, "Planner should preserve the source line on the block");
  require(block->steps[0] == 50, "Planner should convert target distance to step count");
  require(block->steps_event_count == 50, "Planner should record the max step event count");
  require(block->tick_info[0].steps_to_move == 50, "Planner should prepare StepTicker tick info");

  return 0;
}
