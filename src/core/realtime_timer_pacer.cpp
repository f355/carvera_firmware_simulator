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

#include "sim/realtime_timer_pacer.hpp"

#include <algorithm>
#include <thread>

#include "sim/simulator_context.hpp"
#include "sim/virtual_clock.hpp"

extern std::uint32_t SystemCoreClock;

namespace {

std::chrono::nanoseconds cycles_to_duration(std::uint32_t cycles) {
  const std::uint64_t pclk_hz = std::max<std::uint32_t>(1, SystemCoreClock / 4);
  const auto nanos = (static_cast<std::uint64_t>(cycles) * 1'000'000'000ULL + pclk_hz - 1) / pclk_hz;
  return std::chrono::nanoseconds(nanos);
}

}  // namespace

namespace sim {

void RealtimeTimerPacer::reset() { next_deadline_.reset(); }

void RealtimeTimerPacer::pace_cycles(std::uint32_t cycles) {
  if (!clock::active().is_realtime()) {
    reset();
    return;
  }

  const auto now = SteadyClock::now();
  if (!next_deadline_.has_value()) {
    next_deadline_ = now;
  }

  *next_deadline_ += cycles_to_duration(cycles);
  if (*next_deadline_ > now) {
    std::this_thread::sleep_until(*next_deadline_);
    return;
  }

  if (now - *next_deadline_ > std::chrono::milliseconds(100)) {
    next_deadline_ = now;
  }
}

namespace realtime_timer_pacer {

RealtimeTimerPacer& active() { return simulator_context::active().realtime_timer_pacer(); }

void reset() { active().reset(); }

}  // namespace realtime_timer_pacer

}  // namespace sim
