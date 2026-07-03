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

#include "sim/simulation_instance.hpp"

#include "sim/simulator_context.hpp"

namespace sim {

SimulationInstance::SimulationInstance() : machine_(persistent_state_), firmware_(machine_) {}

SimulationInstance::SimulationInstance(const PersistentMachineConfig& config)
    : persistent_state_(config), machine_(persistent_state_), firmware_(machine_) {}

MachineSimulator& SimulationInstance::machine() { return machine_; }

const MachineSimulator& SimulationInstance::machine() const { return machine_; }

FirmwareRuntime& SimulationInstance::firmware() { return firmware_; }

RuntimePhysicalControls& SimulationInstance::inputs() { return firmware_.inputs(); }

PhysicalScene& SimulationInstance::world() { return machine_.context().physical_scene(); }

const PhysicalScene& SimulationInstance::world() const { return machine_.context().physical_scene(); }

RuntimeIo& SimulationInstance::io() { return firmware_.io(); }

RuntimePump& SimulationInstance::runner() { return firmware_.runner(); }

PersistentMachineState& SimulationInstance::persistent_state() { return persistent_state_; }

const PersistentMachineState& SimulationInstance::persistent_state() const { return persistent_state_; }

}  // namespace sim
