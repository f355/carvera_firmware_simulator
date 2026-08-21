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

#include "sim/event_engine.hpp"

#include "Conveyor.h"
#include "libs/Kernel.h"
#include "modules/communication/SerialConsole.h"
#include "sim/machine_simulator.hpp"
#include "sim/simulator_context.hpp"
#include "sim/system_reset.hpp"

namespace sim {
namespace {

constexpr std::size_t timer_budget_idle_interval = 1'000;

}  // namespace

EventEngine::EventEngine(MachineSimulator& simulator) : simulator_(simulator) {}

EventRunResult EventEngine::run(Kernel& kernel, const EventRunOptions& options, ResetCallback reset) {
  EventRunResult result;

  for (std::size_t i = 0; i < options.main_loop_iterations; ++i) {
    simulator_.context().clock().advance_us(options.main_loop_interval_us);
    if (!run_firmware_iteration(kernel, result, reset)) {
      return result;
    }
  }

  if (options.max_timer_events == 0) {
    return result;
  }
  if (kernel.conveyor == nullptr) {
    result.motion_idle = false;
    result.status = EventRunStatus::NoProgress;
    return result;
  }

  const bool spend_full_budget = options.timer_budget_policy == TimerBudgetPolicy::SpendFullBudget;
  kernel.conveyor->force_queue();
  result.motion_idle = kernel.conveyor->is_idle();
  if (result.motion_idle && !spend_full_budget) {
    return result;
  }

  for (std::size_t event = 0; event < options.max_timer_events; ++event) {
    if (!run_one_timer_event(kernel)) {
      result.status = EventRunStatus::NoProgress;
      return result;
    }
    ++result.timer_events;

    kernel.conveyor->on_idle(nullptr);
    kernel.conveyor->force_queue();
    result.motion_idle = kernel.conveyor->is_idle();

    if (spend_full_budget && result.timer_events % timer_budget_idle_interval == 0 &&
        !run_firmware_iteration(kernel, result, reset)) {
      return result;
    }

    if (result.motion_idle && !spend_full_budget) {
      result.status = EventRunStatus::ConditionReached;
      return result;
    }
  }

  result.status = spend_full_budget ? EventRunStatus::ConditionReached : EventRunStatus::BudgetExhausted;
  return result;
}

EventRunResult EventEngine::run_until_motion_idle(Kernel& kernel, std::size_t max_timer_events) {
  EventRunOptions options;
  options.max_timer_events = max_timer_events;
  return run(kernel, options);
}

bool EventEngine::run_one_timer_event(Kernel& kernel) {
  auto& context = simulator_.context();
  const bool advanced = context.timer_scheduler().advance_to_next_match();
  service_physical_model(kernel);
  return advanced;
}

bool EventEngine::run_firmware_iteration(Kernel& kernel, EventRunResult& result, const ResetCallback& reset) {
  if (kernel.serial != nullptr && kernel.serial->serial != nullptr) {
    kernel.serial->serial->service_rx_irq();
  }
  kernel.call_event(ON_MAIN_LOOP);
  kernel.call_event(ON_IDLE);
  simulator_.poll();
  ++result.main_loop_iterations;
  if (!system_reset::consume_requested()) {
    return true;
  }

  if (reset) {
    reset();
  }
  result.status = EventRunStatus::Reset;
  result.reset_requested = true;
  result.motion_idle = true;
  return false;
}

void EventEngine::service_physical_model(Kernel& kernel) {
  auto& context = simulator_.context();
  auto& axes = context.stepper_axes();
  auto& scene = context.physical_scene();
  if (axes.count() >= 3) {
    const Point3 spindle_position{
        axes.position_mm(0),
        axes.position_mm(1),
        axes.position_mm(2),
    };
    scene.update_probe_contacts(spindle_position);
    if (axes.count() > 4) {
      scene.update_atc_clamp_position(spindle_position, axes.position_mm(4));
    }
  }
  if (const auto crash_axis = scene.stock_probe_crash_axis(); crash_axis.has_value()) {
    context.motor_alarm_wiring().drive(*crash_axis, true);
  }
  context.motion_telemetry().observe(context, kernel);
}

}  // namespace sim
