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

#ifndef SIMULATOR_SIM_API_CONVERSIONS_HPP
#define SIMULATOR_SIM_API_CONVERSIONS_HPP

#include <cstddef>
#include <optional>
#include <string>

#include "carvera_sim.pb.h"
#include "sim/firmware_runtime.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/physical_scene.hpp"

namespace sim::api {

Box box_from_proto(const carvera::sim::v1::Box& box);
std::optional<std::size_t> axis_index(carvera::sim::v1::Axis axis);
std::optional<LimitSwitchSide> limit_switch_side(carvera::sim::v1::LimitSwitchSide side);
std::optional<TemperatureSensor> temperature_sensor(carvera::sim::v1::TemperatureSensor sensor);
std::optional<MachineSwitch> machine_switch(carvera::sim::v1::SwitchName name);
ToolKind tool_kind(carvera::sim::v1::ToolKind kind);
carvera::sim::v1::ToolKind proto_tool_kind(ToolKind kind);
bool valid_pin(const carvera::sim::v1::PinAddress& pin);
PinAddress pin_address(const carvera::sim::v1::PinAddress& pin);
carvera::sim::v1::TimeMode time_mode(bool realtime);
carvera::sim::v1::MachineModel proto_machine_model(MachineModel model);
MachineModel machine_model(carvera::sim::v1::MachineModel model);
const char* axis_letter(carvera::sim::v1::Axis axis);
std::string jog_command(const carvera::sim::v1::Jog& jog);

}  // namespace sim::api

#endif
