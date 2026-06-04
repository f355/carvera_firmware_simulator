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

#include <algorithm>
#include <cmath>
#include <limits>

#include "sim/simulator_context.hpp"

namespace sim {
void VirtualClock::reset() {
  mode_ = Mode::Manual;
  base_us_ = 0;
  realtime_started_at_ = {};
  realtime_speed_ = 1.0;
}

void VirtualClock::advance_us(std::uint64_t delta_us) {
  if (is_realtime()) {
    pause_realtime();
  }
  base_us_ += delta_us;
}

void VirtualClock::start_realtime() {
  if (is_realtime()) {
    return;
  }

  realtime_started_at_ = SteadyClock::now();
  mode_ = Mode::Realtime;
}

void VirtualClock::pause_realtime() {
  if (!is_realtime()) {
    return;
  }

  base_us_ = read_us();
  mode_ = Mode::Manual;
  realtime_started_at_ = {};
}

bool VirtualClock::set_realtime_speed(double speed) {
  if (!std::isfinite(speed) || speed <= 0.0 || speed > 100.0) {
    return false;
  }

  if (is_realtime()) {
    base_us_ = read_us();
    realtime_started_at_ = SteadyClock::now();
  }
  realtime_speed_ = speed;
  return true;
}

std::uint64_t VirtualClock::read_us() const {
  if (!is_realtime()) {
    return base_us_;
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(SteadyClock::now() - realtime_started_at_);
  const auto scaled_elapsed = static_cast<long double>(elapsed.count()) * realtime_speed_;
  const auto max_delta = static_cast<long double>(std::numeric_limits<std::uint64_t>::max() - base_us_);
  return base_us_ + static_cast<std::uint64_t>(std::min(scaled_elapsed, max_delta));
}

VirtualClock& clock::active() { return simulator_context::active().clock(); }

void clock::reset() { active().reset(); }

}  // namespace sim
