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

#include "sim/runtime_motor_alarm_wiring.hpp"

#include "Pin.h"
#include "sim/gpio_level.hpp"
#include "sim/runtime_checksums.hpp"
#include "sim/runtime_pin_config.hpp"

namespace sim::runtime_motor_alarm_wiring {

void MotorAlarmWiring::clear() { alarm_signals_ = {}; }

void MotorAlarmWiring::set(std::size_t axis, PinAddress pin, bool active_level) {
  if (axis >= alarm_signals_.size()) {
    return;
  }
  alarm_signals_[axis] = AlarmSignal{pin, active_level, true};
}

void MotorAlarmWiring::drive(std::size_t axis, bool triggered) const {
  if (axis >= alarm_signals_.size() || !alarm_signals_[axis].connected) {
    return;
  }
  const auto& signal = alarm_signals_[axis];
  gpio::set_level(signal.pin, triggered ? signal.active_level : !signal.active_level);
}

void configure(Kernel& kernel, MotorAlarmWiring& wiring) {
  wiring.clear();
  for (std::size_t axis = 0; axis < runtime_checksums::motor_alarm_count; ++axis) {
    Pin alarm_pin = runtime_pins::configured_pin(kernel, runtime_checksums::motor_alarm[axis], "nc");
    if (!alarm_pin.connected()) {
      continue;
    }
    wiring.set(axis, runtime_pins::pin_address(alarm_pin), runtime_pins::raw_level_for_pin_get(alarm_pin, true));
  }
}

}  // namespace sim::runtime_motor_alarm_wiring
