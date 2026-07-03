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

#include "sim/motion_runner.hpp"

#include "Conveyor.h"
#include "libs/Kernel.h"
#include "sim/motion_pump.hpp"
#include "sim/machine_simulator.hpp"

namespace sim {

MotionRunner::MotionRunner(MachineSimulator& simulator, Kernel& kernel) : simulator_(simulator), kernel_(kernel) {}

bool MotionRunner::run_until_idle(std::size_t max_step_ticks) {
  if (kernel_.conveyor == nullptr) {
    return false;
  }

  kernel_.conveyor->force_queue();

  for (std::size_t tick = 0; tick < max_step_ticks; ++tick) {
    pump_motion(simulator_.context(), kernel_);
    kernel_.conveyor->on_idle(nullptr);
    kernel_.conveyor->force_queue();

    if (kernel_.conveyor->is_idle()) {
      return true;
    }
  }

  return kernel_.conveyor->is_idle();
}

}  // namespace sim
