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

#include "sim/motion_telemetry.hpp"

#include <cmath>
#include <utility>

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/simulator_context.hpp"
#include "sim/stepper_axis.hpp"
#include "sim/virtual_clock.hpp"

namespace sim {

namespace {

MachineModel machine_model_from_kernel(Kernel& kernel) {
  if (kernel.factory_set != nullptr && kernel.factory_set->MachineModel == CARVERA_AIR) {
    return MachineModel::CarveraAirCA1;
  }
  return MachineModel::CarveraC1;
}

}  // namespace

void MotionTelemetry::reset() {
  ticks_since_emit_ = 0;
  next_sequence_ = 1;
  last_observed_steps_.clear();
  last_emitted_spindle_rpm_.reset();
  last_emitted_spindle_target_rpm_.reset();
  last_emitted_time_us_.reset();
  last_emitted_steps_.reset();
}

void MotionTelemetry::set_sink(Sink sink) {
  sink_ = std::move(sink);
  reset();
}

void MotionTelemetry::set_interval_ticks(std::size_t ticks) { interval_ticks_ = ticks == 0 ? 1 : ticks; }

void MotionTelemetry::set_interval_us(std::uint64_t interval_us) { interval_us_ = interval_us == 0 ? 1 : interval_us; }

void MotionTelemetry::observe(Kernel& kernel, bool force) {
  if (!sink_) {
    return;
  }

  constexpr std::size_t max_stock_axis_count = 5;
  const auto axis_count = std::min(max_stock_axis_count, stepper_axes::count());
  std::vector<std::int64_t> steps;
  steps.reserve(axis_count);
  for (std::size_t i = 0; i < axis_count; ++i) {
    steps.push_back(stepper_axes::position_steps(i));
  }

  if (!force && last_observed_steps_.empty() && !steps.empty()) {
    last_observed_steps_ = steps;
    return;
  }

  if (!force) {
    ++ticks_since_emit_;
  }
  const auto now_us = clock::active().read_us();

  const auto model = machine_model_from_kernel(kernel);
  const auto spindle = spindle_state::snapshot(model);
  const bool changed_since_last_observation = steps != last_observed_steps_;
  const bool spindle_changed_since_last_emit =
      !last_emitted_spindle_rpm_.has_value() || std::fabs(spindle.actual_rpm - *last_emitted_spindle_rpm_) >= 10.0 ||
      spindle.spinning != (*last_emitted_spindle_rpm_ > 0.5) || !last_emitted_spindle_target_rpm_.has_value() ||
      std::fabs(spindle.target_rpm - *last_emitted_spindle_target_rpm_) >= 10.0;
  last_observed_steps_ = steps;
  if (!force && !changed_since_last_observation && !spindle_changed_since_last_emit) {
    return;
  }

  const bool tick_interval_elapsed = ticks_since_emit_ >= interval_ticks_;
  const bool time_interval_elapsed = !last_emitted_time_us_.has_value() || now_us < *last_emitted_time_us_ ||
                                     now_us - *last_emitted_time_us_ >= interval_us_;
  const bool interval_elapsed =
      changed_since_last_observation ? (tick_interval_elapsed || time_interval_elapsed) : time_interval_elapsed;
  if (!force && !interval_elapsed && last_emitted_steps_.has_value()) {
    return;
  }
  if (!force && last_emitted_steps_.has_value() && last_emitted_spindle_rpm_.has_value() &&
      last_emitted_spindle_target_rpm_.has_value() && steps == *last_emitted_steps_ &&
      std::fabs(spindle.actual_rpm - *last_emitted_spindle_rpm_) < 10.0 &&
      std::fabs(spindle.target_rpm - *last_emitted_spindle_target_rpm_) < 10.0) {
    return;
  }
  ticks_since_emit_ = 0;
  last_emitted_steps_ = steps;
  last_emitted_spindle_rpm_ = spindle.actual_rpm;
  last_emitted_spindle_target_rpm_ = spindle.target_rpm;
  last_emitted_time_us_ = now_us;

  MachineTelemetry sample;
  sample.sequence = next_sequence_++;
  sample.time_us = now_us;
  MachineStateSnapshotOptions options;
  options.axis_count = axis_count;
  static_cast<MachineStateSnapshot&>(sample) = assemble_machine_state(kernel, model, options);

  sink_(sample);
}

namespace motion_telemetry {

MotionTelemetry& active() { return simulator_context::active().motion_telemetry(); }

void reset() { active().reset(); }

}  // namespace motion_telemetry

}  // namespace sim
