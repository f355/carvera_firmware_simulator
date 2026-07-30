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
#include <string>
#include <thread>

#include "StepTicker.h"
#include "libs/Kernel.h"
#include "sim/event_engine.hpp"
#include "sim/lpc1768.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/realtime_timer_pacer.hpp"
#include "support/assertions.hpp"

using sim::test::require;

int main() {
  sim::MachineSimulator simulator;
  sim::EventEngine engine(simulator);
  simulator.start_realtime();
  simulator.set_realtime_speed(4.0);

  const auto unpumped_time = simulator.time_us();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  require(simulator.time_us() == unpumped_time,
          "realtime pacing should not advance firmware time without emulated work");

  simulator.set_realtime_speed(1.0);
  const auto before_speed_change = simulator.time_us();
  simulator.set_realtime_speed(2.0);
  const auto after_speed_change = simulator.time_us();
  require(after_speed_change >= before_speed_change && after_speed_change - before_speed_change < 10'000,
          "changing realtime speed should preserve virtual time continuity");

  Kernel kernel;
  kernel.step_ticker->start();

  auto& timer0 = sim::lpc1768::timer(0);
  timer0.MR0 = 250'000;  // 10 ms at the LPC1768 timer peripheral clock.
  timer0.TCR = 1;

  const auto measure_timer_events = [&](double speed) {
    simulator.set_realtime_speed(speed);
    sim::realtime_timer_pacer::reset();
    const auto started = std::chrono::steady_clock::now();
    for (int event = 0; event < 8; ++event) {
      engine.run_one_timer_event(kernel);
    }
    return std::chrono::steady_clock::now() - started;
  };
  const auto baseline_elapsed = measure_timer_events(1.0);
  const auto accelerated_motion_elapsed = measure_timer_events(2.0);

  const auto baseline_us = std::chrono::duration_cast<std::chrono::microseconds>(baseline_elapsed).count();
  const auto accelerated_us = std::chrono::duration_cast<std::chrono::microseconds>(accelerated_motion_elapsed).count();
  require(accelerated_motion_elapsed * 4 < baseline_elapsed * 3,
          "realtime motion pumping should scale Timer0 wall-clock pacing by the speed multiplier; baseline=" +
              std::to_string(baseline_us) + " us, accelerated=" + std::to_string(accelerated_us) + " us");

  return 0;
}
