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

#ifndef SIMULATOR_SIM_RUNTIME_MOTOR_ALARM_WIRING_HPP
#define SIMULATOR_SIM_RUNTIME_MOTOR_ALARM_WIRING_HPP

#include <array>
#include <cstddef>

#include "sim/pin_address.hpp"
#include "sim/runtime_checksums.hpp"

class Kernel;

namespace sim::runtime_motor_alarm_wiring {

struct AlarmSignal {
  PinAddress pin{};
  bool active_level{true};
  bool connected{false};
};

class MotorAlarmWiring {
 public:
  void clear();
  void set(std::size_t axis, PinAddress pin, bool active_level);
  void drive(std::size_t axis, bool triggered) const;

 private:
  std::array<AlarmSignal, runtime_checksums::motor_alarm_count> alarm_signals_{};
};

void configure(Kernel& kernel);
void drive(std::size_t axis, bool triggered);

}  // namespace sim::runtime_motor_alarm_wiring

#endif
