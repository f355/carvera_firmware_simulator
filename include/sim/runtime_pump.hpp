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

#ifndef SIMULATOR_SIM_RUNTIME_PUMP_HPP
#define SIMULATOR_SIM_RUNTIME_PUMP_HPP

#include <cstddef>

namespace sim {

class MachineSimulator;
class RuntimeBootSession;

enum class TimerBudgetMode {
  StopWhenMotionIdle,
  SpendFullBudget,
};

struct RuntimePumpOptions {
  std::size_t main_loop_iterations{0};
  std::size_t max_step_ticks{0};
  bool drain_serial_lines{false};
  // Free-running mode must spend the whole timer budget even when no steppers
  // are moving, otherwise firmware SlowTicker users stop seeing time pass.
  TimerBudgetMode timer_budget_mode{TimerBudgetMode::StopWhenMotionIdle};
};

struct RuntimePumpResult {
  bool motion_idle{true};
  bool reset_requested{false};
};

class RuntimePump {
 public:
  RuntimePump(MachineSimulator& simulator, RuntimeBootSession& boot_session);

  RuntimePumpResult pump(const RuntimePumpOptions& options);
  void run_main_loop(std::size_t iterations);
  bool run_until_idle(std::size_t max_step_ticks);
  bool pump_free_running(std::size_t main_loop_iterations = 4, std::size_t max_step_ticks = 1000);

 private:
  MachineSimulator& simulator_;
  RuntimeBootSession& boot_session_;
};

}  // namespace sim

#endif
