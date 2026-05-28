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

#ifndef SIMULATOR_SIM_MACHINE_STATE_PROTO_HPP
#define SIMULATOR_SIM_MACHINE_STATE_PROTO_HPP

#include "carvera_sim.pb.h"
#include "sim/machine_state_snapshot.hpp"

namespace sim::api {

void fill_box_proto(carvera::sim::v1::Box& target, const Box& source);
void fill_axis_state_proto(carvera::sim::v1::AxisState& target, const AxisMachineState& source);
void fill_spindle_state_proto(carvera::sim::v1::SpindleState& target, const spindle_state::Snapshot& source);
void fill_atc_state_proto(carvera::sim::v1::AtcState& target, const MachineStateSnapshot& source);
void fill_machine_snapshot_proto(carvera::sim::v1::MachineSnapshot& target, const MachineStateSnapshot& source);
void fill_machine_telemetry_proto(carvera::sim::v1::MachineTelemetry& target, const MachineStateSnapshot& source);

}  // namespace sim::api

#endif
