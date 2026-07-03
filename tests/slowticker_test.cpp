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

#include "gpio.h"
#include "libs/Kernel.h"
#include "libs/SlowTicker.h"
#include "lpc1768_sim.h"

extern "C" void TIMER2_IRQHandler(void);

GPIO leds[4] = {
    GPIO(P4_29),
    GPIO(P4_28),
    GPIO(P0_4),
    GPIO(P1_17),
};

#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

struct Probe {
  uint32_t calls{0};

  uint32_t tick(uint32_t) {
    ++calls;
    return 0;
  }
};

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::lpc1768::reset();

  Kernel kernel;
  SlowTicker& ticker = *kernel.slow_ticker;
  Probe probe;

  ticker.attach(5, &probe, &Probe::tick);
  ticker.start();

  require((sim::lpc1768::sc().PCONP & (1u << 22)) != 0, "Timer2 power bit should be enabled");
  require(sim::lpc1768::timer(2).MCR == 3, "Timer2 should reset on MR0");
  require(sim::lpc1768::timer(2).MR0 == (SystemCoreClock / 4 / 5), "Timer2 match should be configured for 5 Hz");
  require(sim::lpc1768::irq_enabled(TIMER2_IRQn), "Timer2 IRQ should be enabled after start()");

  TIMER2_IRQHandler();
  require(probe.calls == 0, "SlowTicker hook should not fire on the first base tick");

  TIMER2_IRQHandler();
  require(probe.calls == 1, "SlowTicker hook should fire on the second base tick");

  return 0;
}
