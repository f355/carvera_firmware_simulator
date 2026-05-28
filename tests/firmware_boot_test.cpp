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

#include "StepTicker.h"
#include "libs/Kernel.h"
#include "lpc1768_sim.h"
#include "sim/firmware_boot_stubs.hpp"

void init();

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::lpc1768::reset();
  sim::boot::reset_boot_stubs();

  init();

  require(THEKERNEL != nullptr, "firmware init() should create Kernel");
  require(THEKERNEL->conveyor != nullptr && THEKERNEL->conveyor->queue.length > 0,
          "firmware init() should start conveyor and allocate its queue");
  require(THEKERNEL->step_ticker != nullptr, "firmware init() should create step ticker");
  require(StepTicker::getInstance() == THEKERNEL->step_ticker, "firmware init() should install step ticker singleton");
  require(sim::lpc1768::irq_enabled(TIMER0_IRQn), "firmware init() should enable Timer0 IRQ");
  require(sim::lpc1768::irq_enabled(TIMER1_IRQn), "firmware init() should enable Timer1 IRQ");
  require(THEKERNEL->slow_ticker != nullptr, "firmware init() should create slow ticker");
  require(sim::lpc1768::irq_enabled(TIMER2_IRQn), "firmware init() should enable Timer2 IRQ");
  require(sim::boot::loaded_module_count() > 0, "firmware init() should load modules");

  return 0;
}
