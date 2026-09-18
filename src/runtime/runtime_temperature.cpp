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

#include <cmath>

#include "Config.h"
#include "ConfigValue.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "modules/tools/temperaturecontrol/TemperatureControlPublicAccess.h"
#include "sim/machine_simulator.hpp"

extern "C" void TIMER2_IRQHandler(void);

namespace {

constexpr int kMaximumRefreshTicks = 4096;
constexpr float kTemperatureToleranceCelsius = 0.5F;
constexpr std::uint16_t kEnableChecksum = CHECKSUM("enable");

std::uint16_t temperature_checksum(sim::TemperatureSensor sensor) {
  return sensor == sim::TemperatureSensor::Spindle ? spindle_temperature_checksum : power_temperature_checksum;
}

bool temperature_control_enabled(sim::TemperatureSensor sensor) {
  return THEKERNEL != nullptr && THEKERNEL->config != nullptr &&
         THEKERNEL->config->value(temperature_control_checksum, temperature_checksum(sensor), kEnableChecksum)
             ->as_bool(false);
}

}  // namespace

namespace sim::runtime_temperature {

void refresh_temperature_reading(TemperatureSensor sensor, double expected_celsius) {
  if (!temperature_control_enabled(sensor)) {
    return;
  }
  // Exercise the real SlowTicker hook until the firmware's cached reading has
  // converged. The number of IRQs per temperature sample depends on the
  // highest configured SlowTicker frequency, so a fixed sample or wall-clock
  // delay is not reliable across machine configurations.
  for (int i = 0; i < kMaximumRefreshTicks; ++i) {
    TIMER2_IRQHandler();
    pad_temperature reading{};
    if (PublicData::get_value(temperature_control_checksum, current_temperature_checksum, temperature_checksum(sensor),
                              &reading) &&
        std::isfinite(reading.current_temperature) &&
        std::abs(reading.current_temperature - static_cast<float>(expected_celsius)) <= kTemperatureToleranceCelsius) {
      return;
    }
  }
}

}  // namespace sim::runtime_temperature
