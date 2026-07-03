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

#ifndef SIMULATOR_SIM_SIMULATION_INSTANCE_HPP
#define SIMULATOR_SIM_SIMULATION_INSTANCE_HPP

#include "sim/firmware_runtime.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/persistent_machine_state.hpp"

namespace sim {

class SimulationInstance {
 public:
  SimulationInstance();
  explicit SimulationInstance(const PersistentMachineConfig& config);

  SimulationInstance(const SimulationInstance&) = delete;
  SimulationInstance& operator=(const SimulationInstance&) = delete;

  MachineSimulator& machine();
  const MachineSimulator& machine() const;
  FirmwareRuntime& firmware();
  PersistentMachineState& persistent_state();
  const PersistentMachineState& persistent_state() const;

 private:
  PersistentMachineState persistent_state_;
  MachineSimulator machine_;
  FirmwareRuntime firmware_;
};

}  // namespace sim

#endif
