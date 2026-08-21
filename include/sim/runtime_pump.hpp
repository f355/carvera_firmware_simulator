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

#include "sim/event_engine.hpp"

namespace sim {

class EventEngine;
class MachineSimulator;
class RuntimeBootSession;

using TimerBudgetMode = TimerBudgetPolicy;
using RuntimePumpOptions = EventRunOptions;
using RuntimePumpResult = EventRunResult;

class RuntimePump {
 public:
  RuntimePump(MachineSimulator& simulator, EventEngine& engine, RuntimeBootSession& boot_session);

  RuntimePumpResult pump(const RuntimePumpOptions& options);
  void run_main_loop(std::size_t iterations);
  RuntimePumpResult run_until_motion_idle(std::size_t max_timer_events);
  bool pump_free_running(std::size_t main_loop_iterations = 4, std::size_t max_step_ticks = 1000);

 private:
  MachineSimulator& simulator_;
  EventEngine& engine_;
  RuntimeBootSession& boot_session_;
};

}  // namespace sim

#endif
