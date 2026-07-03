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

#include "sim/persistent_machine_state.hpp"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"

int main() {
  sim::SimulationInstance simulation;

  sim::test::require(!simulation.firmware().booted(), "new simulation firmware should start powered off");
  sim::test::require(simulation.machine().set_realtime_speed(3.0), "simulation machine should accept realtime speed");
  sim::test::require(simulation.firmware().realtime_speed() == 3.0,
                     "simulation firmware should be bound to the instance machine");
  const auto& const_simulation = simulation;
  sim::test::require(&const_simulation.machine() == &simulation.machine(),
                     "const and mutable machine access should refer to the owned machine");

  sim::test::TempDirectory sd_root("carvera_sim_persistent_instance_test");
  sim::PersistentMachineConfig config;
  config.mounts.push_back({"sd", sd_root.path()});
  sim::SimulationInstance persistent_simulation(config);

  const auto translated = persistent_simulation.persistent_state().host_filesystem().translate("/sd/config.txt");
  sim::test::require(translated == sd_root.path() / "config.txt",
                     "configured storage should be mounted before the simulation powers on");
  sim::test::require(persistent_simulation.persistent_state().eeprom().has_persistent_file(),
                     "an SD mount should attach its EEPROM backing file");

  persistent_simulation.persistent_state().eeprom().poke(7, 0x5a);
  persistent_simulation.machine().reset();
  sim::test::require(persistent_simulation.persistent_state().eeprom().peek(7) == 0x5a,
                     "machine reset should preserve persistent EEPROM contents");

  return 0;
}
