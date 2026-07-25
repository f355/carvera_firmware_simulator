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

#ifndef SIMULATOR_TESTS_SUPPORT_WAITING_HPP
#define SIMULATOR_TESTS_SUPPORT_WAITING_HPP

#include <chrono>
#include <thread>

namespace sim::test {

// Wall-clock wait that pumps between predicate checks and re-checks once after
// the deadline so a slow pump cannot miss a satisfied condition.
template <typename Predicate, typename Pump>
bool wait_pumping(Predicate&& predicate, Pump&& pump, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    pump();
  }
  return predicate();
}

template <typename Predicate>
bool eventually(Predicate&& predicate, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  return wait_pumping(predicate, [] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }, timeout);
}

}  // namespace sim::test

#endif
