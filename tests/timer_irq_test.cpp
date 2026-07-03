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
#undef protected
#undef private

#include "Block.h"
#include "Pin.h"
#include "StepTicker.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/timer_irq.hpp"
#include "support/assertions.hpp"

using sim::test::require;


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
  const int motor_index = kernel.step_ticker->register_motor(&motor);
  require(motor_index == 0, "first registered motor should get index zero");

  kernel.conveyor->start(1);
  Block* block = kernel.conveyor->queue.head_ref();
  block->tick_info[motor_index].steps_to_move = 1;
  block->tick_info[motor_index].steps_per_tick = STEPTICKER_FPSCALE;
  block->direction_bits.set(motor_index, true);
  block->ready();
  kernel.conveyor->queue_head_block();
  kernel.conveyor->force_queue();

  sim::timer_irq::dispatch_match(0);
  require(simulator.axis_position_steps(axis) == 0, "Timer0 match should not dispatch while Timer0 IRQ is disabled");

  kernel.step_ticker->start();
  sim::timer_irq::dispatch_match(0);
  require(simulator.axis_position_steps(axis) == -1,
          "Timer0 match should dispatch StepTicker when Timer0 IRQ is enabled");
  require(step_pin.get(), "Timer0 match should leave the step pin active until Timer1 match");
  require(sim::lpc1768::timer(1).TCR == 1, "StepTicker should arm Timer1 after stepping");

  sim::timer_irq::dispatch_match(1);
  require(!step_pin.get(), "Timer1 match should dispatch unstep through StepTicker");
  require(sim::lpc1768::timer(1).TCR == 0, "Timer1 match should honor the stop-on-match bit in MCR");

  block = kernel.conveyor->queue.head_ref();
  block->tick_info[motor_index].steps_to_move = 1;
  block->tick_info[motor_index].steps_per_tick = STEPTICKER_FPSCALE;
  block->direction_bits.set(motor_index, true);
  block->ready();
  kernel.conveyor->queue_head_block();
  kernel.conveyor->force_queue();

  const auto timer0_period = sim::lpc1768::timer(0).MR0;
  const auto timer1_delay = sim::lpc1768::timer(1).MR0;
  sim::timer_irq::advance_cycles(timer0_period - 1);
  require(simulator.axis_position_steps(axis) == -1,
          "elapsed timer cycles should not dispatch Timer0 before MR0 is reached");

  sim::timer_irq::advance_cycles(1);
  require(simulator.axis_position_steps(axis) == -2,
          "elapsed timer cycles should dispatch Timer0 exactly when MR0 is reached");
  require(step_pin.get(), "Timer1 should be armed but not elapsed during the same Timer0 cycle slice");

  sim::timer_irq::advance_cycles(timer1_delay - 1);
  require(step_pin.get(), "Timer1 should not unstep before its match delay elapses");

  sim::timer_irq::advance_cycles(1);
  require(!step_pin.get(), "Timer1 should unstep after its own elapsed match delay");

  return 0;
}
