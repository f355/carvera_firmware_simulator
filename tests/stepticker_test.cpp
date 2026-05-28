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
#include <cstdlib>
#include <iostream>

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
#include "lpc1768_sim.h"
#include "sim/machine_simulator.hpp"

extern "C" void TIMER0_IRQHandler(void);
extern "C" void TIMER1_IRQHandler(void);

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

  require(kernel.step_ticker != nullptr, "Kernel should create a StepTicker");
  require(StepTicker::getInstance() == kernel.step_ticker, "StepTicker should publish its singleton");
  require(sim::lpc1768::timer(0).MR0 == (SystemCoreClock / 4 / 100000),
          "StepTicker should configure Timer0 from the default stepping frequency");
  require(sim::lpc1768::timer(1).MR0 == (SystemCoreClock / 4 / 1'000'000),
          "StepTicker should configure Timer1 from the default unstep pulse width");
  require((sim::lpc1768::sc().PCONP & (1 << 2)) != 0, "StepTicker should power Timer0/1");

  kernel.step_ticker->start();

  require(sim::lpc1768::irq_enabled(TIMER0_IRQn), "StepTicker start should enable Timer0 IRQ");
  require(sim::lpc1768::irq_enabled(TIMER1_IRQn), "StepTicker start should enable Timer1 IRQ");

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

  TIMER0_IRQHandler();

  require(motor.which_direction(), "StepTicker should set motor direction from the block");
  require(static_cast<int32_t>(motor.get_current_step()) == -1, "StepTicker should issue a step for a ready block");
  require(simulator.axis_position_steps(axis) == -1,
          "physical axis should move from the StepTicker-generated step/dir pulse");
  require(step_pin.get(), "StepTicker should leave the step pin active until the unstep timer fires");
  require(kernel.conveyor->queue.tail_i != kernel.conveyor->queue.isr_tail_i,
          "StepTicker should tell Conveyor when the block finishes");
  require(sim::lpc1768::timer(1).TCR == 1, "StepTicker should start Timer1 to schedule unstep");

  TIMER1_IRQHandler();

  require(!step_pin.get(), "Timer1 ISR should unstep motors stepped by Timer0");

  return 0;
}
