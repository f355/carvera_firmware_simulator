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

#include "sim/event_engine.hpp"
#include "sim/runtime_boot_session.hpp"

namespace sim {
RuntimePump::RuntimePump(EventEngine& engine, RuntimeBootSession& boot_session)
    : engine_(engine), boot_session_(boot_session) {}

RuntimePumpResult RuntimePump::pump(const RuntimePumpOptions& options) {
  auto& kernel = boot_session_.boot();
  auto reset = [this]() { boot_session_.reset(); };
  auto result = engine_.run(kernel, options, reset);
  if (options.max_timer_events == 0 || result.reset_requested) {
    return result;
  }

  EventRunOptions final_iteration;
  final_iteration.main_loop_iterations = 1;
  const auto final_result = engine_.run(kernel, final_iteration, reset);
  result.main_loop_iterations += final_result.main_loop_iterations;
  if (final_result.reset_requested) {
    result.status = final_result.status;
    result.reset_requested = true;
    result.motion_idle = true;
  }
  return result;
}

void RuntimePump::run_main_loop(std::size_t iterations) {
  RuntimePumpOptions options;
  options.main_loop_iterations = iterations;
  pump(options);
}

RuntimePumpResult RuntimePump::run_until_motion_idle(std::size_t max_timer_events) {
  RuntimePumpOptions options;
  options.main_loop_iterations = 64;
  options.max_timer_events = max_timer_events;
  options.drain_serial_lines = true;
  return pump(options);
}

bool RuntimePump::pump_free_running(std::size_t main_loop_iterations, std::size_t max_step_ticks) {
  return pump_free_running_result(main_loop_iterations, max_step_ticks).motion_idle;
}

RuntimePumpResult RuntimePump::pump_free_running_result(std::size_t main_loop_iterations,
                                                        std::size_t max_step_ticks) {
  RuntimePumpOptions options;
  options.main_loop_iterations = main_loop_iterations;
  options.max_timer_events = max_step_ticks;
  options.timer_budget_policy = TimerBudgetMode::SpendFullBudget;
  return pump(options);
}

}  // namespace sim
