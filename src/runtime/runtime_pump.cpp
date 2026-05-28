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

#include "sim/runtime_pump.hpp"

#include <cstdint>

#include "Conveyor.h"
#include "libs/Kernel.h"
#include "modules/communication/SerialConsole.h"
#include "sim/machine_simulator.hpp"
#include "sim/motion_pump.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/system_reset.hpp"

namespace sim {
namespace {

constexpr std::size_t kTimerBudgetIdleInterval = 1'000;

}  // namespace

RuntimePump::RuntimePump(MachineSimulator& simulator, RuntimeBootSession& boot_session)
    : simulator_(simulator), boot_session_(boot_session) {}

RuntimePumpResult RuntimePump::pump(const RuntimePumpOptions& options) {
  auto& kernel = boot_session_.boot();
  const bool spend_full_timer_budget = options.timer_budget_mode == TimerBudgetMode::SpendFullBudget;

  RuntimePumpResult result;
  auto pump_main_loop_once = [&]() {
    kernel.call_event(ON_MAIN_LOOP);
    kernel.call_event(ON_IDLE);
    simulator_.poll();
    if (system_reset::consume_requested()) {
      boot_session_.reset();
      result.reset_requested = true;
      result.motion_idle = true;
      return false;
    }
    return true;
  };

  for (std::size_t i = 0; i < options.main_loop_iterations; ++i) {
    if (options.drain_serial_lines && (kernel.serial == nullptr || !kernel.serial->has_char('\n'))) {
      break;
    }
    if (!pump_main_loop_once()) {
      return result;
    }
  }

  if (options.max_step_ticks == 0) {
    return result;
  }

  if (kernel.conveyor == nullptr) {
    result.motion_idle = false;
    return result;
  }

  kernel.conveyor->force_queue();
  for (std::size_t tick = 0; tick < options.max_step_ticks; ++tick) {
    pump_motion(kernel);
    kernel.conveyor->on_idle(nullptr);
    kernel.conveyor->force_queue();

    if (spend_full_timer_budget && (tick + 1) % kTimerBudgetIdleInterval == 0) {
      kernel.call_event(ON_IDLE);
      simulator_.poll();
      if (system_reset::consume_requested()) {
        boot_session_.reset();
        result.reset_requested = true;
        result.motion_idle = true;
        return result;
      }
    }

    result.motion_idle = kernel.conveyor->is_idle();
    if (result.motion_idle && !spend_full_timer_budget) {
      break;
    }
  }
  result.motion_idle = kernel.conveyor->is_idle();
  if (!pump_main_loop_once()) {
    return result;
  }
  return result;
}

void RuntimePump::run_main_loop(std::size_t iterations) {
  RuntimePumpOptions options;
  options.main_loop_iterations = iterations;
  pump(options);
}

bool RuntimePump::run_until_idle(std::size_t max_step_ticks) {
  RuntimePumpOptions options;
  options.main_loop_iterations = 64;
  options.max_step_ticks = max_step_ticks;
  options.drain_serial_lines = true;
  return pump(options).motion_idle;
}

bool RuntimePump::pump_free_running(std::size_t main_loop_iterations, std::size_t max_step_ticks) {
  RuntimePumpOptions options;
  options.main_loop_iterations = main_loop_iterations;
  options.max_step_ticks = max_step_ticks;
  options.timer_budget_mode = TimerBudgetMode::SpendFullBudget;
  return pump(options).motion_idle;
}

}  // namespace sim
