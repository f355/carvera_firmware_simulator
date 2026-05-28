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

#include "sim/spindle_state.hpp"

#include <algorithm>

#include "sim/simulator_context.hpp"

namespace sim {

namespace {

double clamped_target(MachineModel model, bool enabled, double requested_rpm) {
  if (!enabled || requested_rpm <= 0.0) {
    return 0.0;
  }
  return std::clamp(requested_rpm, 0.0, spindle_state::max_rpm(model));
}

}  // namespace

namespace spindle_state {

double max_rpm(MachineModel model) {
  switch (model) {
    case MachineModel::CarveraAirCA1:
      return 14500.0;
    case MachineModel::CarveraC1:
    default:
      return 15500.0;
  }
}

}  // namespace spindle_state

void SpindleState::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  state_ = State{};
}

void SpindleState::update(MachineModel model, bool enabled, double requested_rpm, std::uint64_t now_us) {
  std::lock_guard<std::mutex> lock(mutex_);
  const double next_target = clamped_target(model, enabled, requested_rpm);
  if (!state_.initialized) {
    state_.initialized = true;
    state_.last_update_us = now_us;
    state_.target_rpm = next_target;
    return;
  }

  const double elapsed_seconds =
      now_us >= state_.last_update_us ? static_cast<double>(now_us - state_.last_update_us) / 1'000'000.0 : 0.0;
  const double max_delta = elapsed_seconds * spindle_state::ramp_rate_rpm_per_second;
  if (state_.actual_rpm < state_.target_rpm) {
    state_.actual_rpm = std::min(state_.actual_rpm + max_delta, state_.target_rpm);
  } else if (state_.actual_rpm > state_.target_rpm) {
    state_.actual_rpm = std::max(state_.actual_rpm - max_delta, state_.target_rpm);
  }
  state_.target_rpm = next_target;
  state_.last_update_us = now_us;
}

spindle_state::Snapshot SpindleState::snapshot(MachineModel model) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return spindle_state::Snapshot{
      state_.actual_rpm > 0.5,
      state_.actual_rpm,
      state_.target_rpm,
      spindle_state::max_rpm(model),
  };
}

double SpindleState::actual_rpm() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_.actual_rpm;
}

double SpindleState::target_rpm() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_.target_rpm;
}

namespace spindle_state {

SpindleState& active() { return simulator_context::active().spindle(); }

void reset() { active().reset(); }

void update(MachineModel model, bool enabled, double requested_rpm, std::uint64_t now_us) {
  active().update(model, enabled, requested_rpm, now_us);
}

Snapshot snapshot(MachineModel model) { return active().snapshot(model); }

double actual_rpm() { return active().actual_rpm(); }

double target_rpm() { return active().target_rpm(); }

}  // namespace spindle_state

}  // namespace sim
