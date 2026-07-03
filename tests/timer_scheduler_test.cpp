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
#include "StepTicker.h"
#undef protected
#undef private

#include "Block.h"
#include "Pin.h"
#include "SlowTicker.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/lpc1768.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/timer_irq.hpp"
#include "us_ticker_api.h"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

int prepare_motor(Kernel& kernel, Pin& step_pin, Pin& dir_pin, Pin& enable_pin) {
  require(step_pin.from_string("1.18") != nullptr, "step pin should parse");
  require(dir_pin.from_string("1.20") != nullptr, "direction pin should parse");
  step_pin.as_output();
  dir_pin.as_output();

  auto* motor = new StepperMotor(step_pin, dir_pin, enable_pin);
  const int motor_index = kernel.step_ticker->register_motor(motor);
  require(motor_index == 0, "first registered motor should get index zero");
  kernel.conveyor->start(1);
  return motor_index;
}

void queue_steps(Kernel& kernel, int motor_index, std::uint32_t steps) {
  Block* block = kernel.conveyor->queue.head_ref();
  block->tick_info[motor_index].steps_to_move = steps;
  block->tick_info[motor_index].steps_per_tick = STEPTICKER_FPSCALE;
  block->direction_bits.set(motor_index, true);
  block->ready();
  kernel.conveyor->queue_head_block();
  kernel.conveyor->force_queue();
}

}  // namespace

int main() {
  {
    sim::MachineSimulator simulator;
    Kernel kernel;
    kernel.step_ticker->set_unstep_time(1);
    kernel.step_ticker->start();

    const auto axis = simulator.add_step_dir_axis({1, 18}, {1, 20});
    Pin step_pin;
    Pin dir_pin;
    Pin enable_pin;
    const int motor_index = prepare_motor(kernel, step_pin, dir_pin, enable_pin);
    queue_steps(kernel, motor_index, 2);

    require(sim::timer_irq::advance_to_next_match(), "Timer0 should be the first timer match");
    require(simulator.axis_position_steps(axis) == -1, "first Timer0 match should step once");
    require(step_pin.get(), "step pulse should remain high until Timer1 match");

    require(sim::timer_irq::advance_to_next_match(), "Timer1 should match before the next Timer0 step");
    require(simulator.axis_position_steps(axis) == -1, "Timer1 match should not add a step");
    require(!step_pin.get(), "Timer1 match should end the step pulse");
    require(sim::lpc1768::timer(0).TC == sim::lpc1768::timer(1).MR0,
            "Timer0 should only advance by the Timer1 pulse width");

    require(sim::timer_irq::advance_to_next_match(), "Timer0 should match again after Timer1");
    require(simulator.axis_position_steps(axis) == -2, "second Timer0 match should produce a second edge");
    require(step_pin.get(), "second step should arm another pulse");
  }

  {
    sim::MachineSimulator simulator;
    Kernel kernel;
    kernel.step_ticker->start();
    NVIC_SetPriority(TIMER0_IRQn, 2);
    NVIC_SetPriority(TIMER1_IRQn, 1);

    const auto axis = simulator.add_step_dir_axis({1, 18}, {1, 20});
    Pin step_pin;
    Pin dir_pin;
    Pin enable_pin;
    const int motor_index = prepare_motor(kernel, step_pin, dir_pin, enable_pin);
    queue_steps(kernel, motor_index, 1);

    require(sim::timer_irq::dispatch_match(0), "first Timer0 match should create a pending unstep");
    require(simulator.axis_position_steps(axis) == -1, "first setup step should move the physical axis");
    require(step_pin.get(), "first setup step should leave the pin high");

    queue_steps(kernel, motor_index, 1);
    auto& timer0 = sim::lpc1768::timer(0);
    auto& timer1 = sim::lpc1768::timer(1);
    timer0.MR0 = 1;
    timer0.TC = 0;
    timer0.TCR = 1;
    timer1.MR0 = 1;
    timer1.TC = 0;
    timer1.TCR = 1;

    sim::timer_irq::advance_cycles(1);

    require(simulator.axis_position_steps(axis) == -2,
            "simultaneous Timer0/Timer1 matches should still produce a new rising edge");
    require(step_pin.get(), "higher-priority Timer1 should run before Timer0 when both matches are pending");
  }

  {
    sim::MachineSimulator simulator;
    Kernel kernel;
    kernel.slow_ticker->start();

    require(us_ticker_read() == 0, "simulator clock should start at zero");
    require(sim::timer_irq::advance_to_next_match(), "SlowTicker should schedule a Timer2 match");
    require(us_ticker_read() == 200000, "advancing the LPC timer should advance us_ticker in manual mode");
  }

  return 0;
}
