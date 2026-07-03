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

#include "test_support.hpp"

#include "sim/simulation_instance.hpp"

int main() {
  sim::SimulationInstance simulation;

  sim::test::require(!simulation.firmware().booted(), "new simulation firmware should start powered off");
  sim::test::require(simulation.machine().set_realtime_speed(3.0), "simulation machine should accept realtime speed");
  sim::test::require(simulation.firmware().realtime_speed() == 3.0,
                     "simulation firmware should be bound to the instance machine");
  const auto& const_simulation = simulation;
  sim::test::require(&const_simulation.machine() == &simulation.machine(),
                     "const and mutable machine access should refer to the owned machine");

  return 0;
}
