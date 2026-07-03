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

#ifndef SIMULATOR_TESTS_SUPPORT_RUNTIME_WAIT_HPP
#define SIMULATOR_TESTS_SUPPORT_RUNTIME_WAIT_HPP

#include <cmath>
#include <cstddef>

#include "sim/machine_simulator.hpp"
#include "sim/runtime_pump.hpp"

#include "libs/Kernel.h"

namespace sim::test {

struct RuntimeWaitBudget {
  int attempts{1000};
  std::size_t main_loop_iterations{8};
  std::size_t max_step_ticks{5'000};
};

template <typename Predicate>
bool pump_until(RuntimePump& runner, Predicate predicate, RuntimeWaitBudget budget = {}) {
  for (int i = 0; i < budget.attempts; ++i) {
    if (predicate()) {
      return true;
    }
    runner.pump_free_running(budget.main_loop_iterations, budget.max_step_ticks);
  }
  return predicate();
}

inline bool pump_until_halted(RuntimePump& runner, Kernel& kernel, RuntimeWaitBudget budget = {}) {
  return pump_until(runner, [&kernel] { return kernel.is_halted(); }, budget);
}

inline bool pump_until_axis_moves_by(RuntimePump& runner, MachineSimulator& simulator, Kernel& kernel, std::size_t axis,
                                     double delta_mm, RuntimeWaitBudget budget = {}) {
  const double start_position = simulator.axis_position_mm(axis);
  return pump_until(
             runner,
             [&simulator, &kernel, axis, start_position, delta_mm] {
               return kernel.is_halted() || std::abs(simulator.axis_position_mm(axis) - start_position) >= delta_mm;
             },
             budget) &&
         !kernel.is_halted();
}

}  // namespace sim::test

#endif
