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

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "sim/i2c_eeprom.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/simulator_context.hpp"
#include "sim/spindle_state.hpp"

namespace {

void require_near(double actual, double expected, double tolerance, const char* message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  auto& spindle = simulator.context().spindle();
  spindle.reset();

  spindle.update(sim::MachineModel::CarveraAirCA1, true, 10'000.0, 0);
  spindle.update(sim::MachineModel::CarveraAirCA1, true, 10'000.0, 1'250'000);
  require_near(spindle.actual_rpm(), 5'000.0, 0.001, "CA1 spindle should ramp up at 4000 RPM/s");

  spindle.update(sim::MachineModel::CarveraAirCA1, true, 10'000.0, 2'500'000);
  require_near(spindle.actual_rpm(), 10'000.0, 0.001,
               "CA1 spindle should reach a 10000 RPM target in 2.5 seconds");

  spindle.update(sim::MachineModel::CarveraAirCA1, true, 30'000.0, 10'000'000);
  require_near(spindle.actual_rpm(), 10'000.0, 0.001,
               "spindle target changes should not spend previous idle time ramping toward the new target");
  spindle.update(sim::MachineModel::CarveraAirCA1, true, 30'000.0, 11'125'000);
  require_near(spindle.actual_rpm(), 14'500.0, 0.001,
               "CA1 spindle should clamp physical speed at its 100 percent duty RPM");
  require_near(spindle.target_rpm(), 14'500.0, 0.001, "CA1 spindle should publish the clamped target RPM");

  spindle.reset();
  spindle.update(sim::MachineModel::CarveraC1, true, 30'000.0, 0);
  spindle.update(sim::MachineModel::CarveraC1, true, 30'000.0, 10'000'000);
  require_near(spindle.actual_rpm(), 15'500.0, 0.001,
               "C1 spindle should clamp physical speed at its 100 percent duty RPM");

  spindle.update(sim::MachineModel::CarveraC1, false, 0.0, 11'000'000);
  require_near(spindle.actual_rpm(), 15'500.0, 0.001,
               "spindle stop should not spend previous running time ramping down before M5 arrives");
  spindle.update(sim::MachineModel::CarveraC1, false, 0.0, 12'000'000);
  require_near(spindle.actual_rpm(), 11'500.0, 0.001,
               "spindle should ramp down linearly instead of stopping tach pulses immediately");

  spindle.reset();
  spindle.update(sim::MachineModel::CarveraC1, false, 0.0, 0);
  spindle.update(sim::MachineModel::CarveraC1, true, 10'000.0, 5'000'000);
  require_near(spindle.actual_rpm(), 0.0, 0.001,
               "spindle should not jump to speed when M3 arrives after a long idle gap");
  spindle.update(sim::MachineModel::CarveraC1, true, 10'000.0, 5'500'000);
  require_near(spindle.actual_rpm(), 2'000.0, 0.001,
               "spindle should ramp from the command arrival time after a long idle gap");

  return 0;
}
