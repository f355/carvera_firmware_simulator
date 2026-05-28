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

#include <chrono>
#include <cstdlib>
#include <iostream>

#include "StepTicker.h"
#include "libs/Kernel.h"
#include "sim/lpc1768.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/motion_pump.hpp"

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
  simulator.start_realtime();
  Kernel kernel;
  kernel.step_ticker->start();

  auto& timer0 = sim::lpc1768::timer(0);
  timer0.MR0 = 25'000;  // 1 ms at the LPC1768 timer peripheral clock.
  timer0.TCR = 1;

  const auto started = std::chrono::steady_clock::now();
  sim::pump_motion(kernel, 2);
  const auto elapsed = std::chrono::steady_clock::now() - started;

  require(elapsed >= std::chrono::microseconds(1500),
          "realtime motion pumping should be paced by Timer0 wall-clock time");

  return 0;
}
