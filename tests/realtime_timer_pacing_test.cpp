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
#include <thread>

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
  simulator.set_realtime_speed(4.0);

  const auto clock_before_sleep = simulator.time_us();
  const auto wall_before_sleep = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  const auto wall_elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - wall_before_sleep);
  const auto accelerated_elapsed = simulator.time_us() - clock_before_sleep;
  const auto minimum_expected = static_cast<std::uint64_t>(wall_elapsed.count() * 3);
  const auto maximum_expected = static_cast<std::uint64_t>(wall_elapsed.count() * 5);
  require(accelerated_elapsed >= minimum_expected && accelerated_elapsed <= maximum_expected,
          "realtime clock should advance by the configured speed multiplier");

  simulator.set_realtime_speed(1.0);
  const auto before_speed_change = simulator.time_us();
  simulator.set_realtime_speed(2.0);
  const auto after_speed_change = simulator.time_us();
  require(after_speed_change >= before_speed_change && after_speed_change - before_speed_change < 10'000,
          "changing realtime speed should preserve virtual time continuity");

  Kernel kernel;
  kernel.step_ticker->start();

  auto& timer0 = sim::lpc1768::timer(0);
  timer0.MR0 = 25'000;  // 1 ms at the LPC1768 timer peripheral clock.
  timer0.TCR = 1;

  simulator.set_realtime_speed(1.0);
  const auto started = std::chrono::steady_clock::now();
  sim::pump_motion(kernel, 2);
  const auto baseline_elapsed = std::chrono::steady_clock::now() - started;

  simulator.set_realtime_speed(2.0);
  const auto accelerated_started = std::chrono::steady_clock::now();
  sim::pump_motion(kernel, 2);
  const auto accelerated_motion_elapsed = std::chrono::steady_clock::now() - accelerated_started;

  require(accelerated_motion_elapsed < baseline_elapsed,
          "realtime motion pumping should scale Timer0 wall-clock pacing by the speed multiplier");

  return 0;
}
