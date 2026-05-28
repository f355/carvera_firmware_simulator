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

#include "sim/runtime_temperature.hpp"

#include "PublicData.h"
#include "modules/tools/temperaturecontrol/TemperatureControlPublicAccess.h"

#include <vector>

namespace sim::runtime_temperature {

void warm_adc_filter() {
  for (int i = 0; i < 4; ++i) {
    std::vector<pad_temperature> controllers;
    PublicData::get_value(temperature_control_checksum, poll_controls_checksum, &controllers);
  }
}

}  // namespace sim::runtime_temperature
