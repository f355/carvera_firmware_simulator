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

#include "sim/virtual_clock.hpp"

#include <cmath>

#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace sim {
void VirtualClock::reset() {
  mode_ = Mode::Manual;
  base_us_ = 0;
  realtime_speed_ = 1.0;
}

void VirtualClock::advance_us(std::uint64_t delta_us) { base_us_ += delta_us; }

void VirtualClock::start_realtime() { mode_ = Mode::Realtime; }

void VirtualClock::pause_realtime() { mode_ = Mode::Manual; }

bool VirtualClock::set_realtime_speed(double speed) {
  if (!std::isfinite(speed) || speed <= 0.0 || speed > 100.0) {
    return false;
  }

  realtime_speed_ = speed;
  return true;
}

std::uint64_t VirtualClock::read_us() const { return base_us_; }

VirtualClock& clock::active() { return compat::active_context().clock(); }

void clock::reset() { active().reset(); }

}  // namespace sim
