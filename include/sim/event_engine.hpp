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

#ifndef SIMULATOR_SIM_EVENT_ENGINE_HPP
#define SIMULATOR_SIM_EVENT_ENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>

class Kernel;

namespace sim {

class MachineSimulator;

enum class EventRunStatus {
  ConditionReached,
  BudgetExhausted,
  Reset,
  NoProgress,
};

enum class TimerBudgetPolicy {
  StopWhenMotionIdle,
  SpendFullBudget,
};

struct EventRunOptions {
  std::size_t main_loop_iterations{0};
  std::size_t max_timer_events{0};
  // Free-running mode spends the complete timer budget so SlowTicker users
  // continue to observe time even when the conveyor is idle.
  TimerBudgetPolicy timer_budget_policy{TimerBudgetPolicy::StopWhenMotionIdle};
  std::uint32_t main_loop_interval_us{0};
};

struct EventRunResult {
  EventRunStatus status{EventRunStatus::ConditionReached};
  bool motion_idle{true};
  bool reset_requested{false};
  std::size_t main_loop_iterations{0};
  std::size_t timer_events{0};
};

class EventEngine {
 public:
  using ResetCallback = std::function<void()>;

  explicit EventEngine(MachineSimulator& simulator);

  EventRunResult run(Kernel& kernel, const EventRunOptions& options, ResetCallback reset = {});
  EventRunResult run_until_motion_idle(Kernel& kernel, std::size_t max_timer_events);
  bool run_one_timer_event(Kernel& kernel);

 private:
  bool run_firmware_iteration(Kernel& kernel, EventRunResult& result, const ResetCallback& reset);
  void service_physical_model(Kernel& kernel);

  MachineSimulator& simulator_;
};

}  // namespace sim

#endif
