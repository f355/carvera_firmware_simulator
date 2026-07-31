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

#ifndef SIMULATOR_SIM_MACHINE_GEOMETRY_HPP
#define SIMULATOR_SIM_MACHINE_GEOMETRY_HPP

#include <array>
#include <optional>

#include "sim/i2c_eeprom.hpp"
#include "sim/physical_scene.hpp"

namespace sim {

struct AxisGeometry {
  double initial_position_mm{0.0};
  double min_switch_mm{0.0};
  double max_switch_mm{0.0};
};

struct MachineGeometry {
  Box physical_travel{};
  // G53 spindle-face coordinate at the physical bed surface.
  double bed_z{0.0};
  Box tool_setter{};
  std::array<AxisGeometry, 3> axes{};
};

std::optional<MachineGeometry> geometry_for(MachineModel model);

}  // namespace sim

#endif
