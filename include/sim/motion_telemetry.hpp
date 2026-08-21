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

#ifndef SIMULATOR_SIM_MOTION_TELEMETRY_HPP
#define SIMULATOR_SIM_MOTION_TELEMETRY_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#include "sim/machine_state_snapshot.hpp"

class Kernel;

namespace sim {

class SimulatorContext;

using AxisTelemetry = AxisMachineState;

struct MachineTelemetry : MachineStateSnapshot {
  std::uint64_t sequence{0};
  std::uint64_t time_us{0};
};

class MotionTelemetry {
 public:
  using Sink = std::function<void(const MachineTelemetry&)>;

  void reset();
  void set_sink(Sink sink);
  void set_interval_ticks(std::size_t ticks);
  void set_interval_us(std::uint64_t interval_us);
  void observe(SimulatorContext& context, Kernel& kernel, bool force = false);

 private:
  struct PhysicalPositionSample {
    std::uint64_t time_us{0};
    std::vector<double> positions{};
  };

  Sink sink_{};
  std::size_t interval_ticks_{30};
  std::uint64_t interval_us_{10'000};
  std::size_t ticks_since_emit_{0};
  std::uint64_t next_sequence_{1};
  std::vector<std::int64_t> last_observed_steps_{};
  std::optional<double> last_emitted_spindle_rpm_{};
  std::optional<double> last_emitted_spindle_target_rpm_{};
  std::optional<std::uint64_t> last_emitted_time_us_{};
  std::optional<std::chrono::steady_clock::time_point> last_emitted_wall_time_{};
  std::optional<std::vector<std::int64_t>> last_emitted_steps_{};
  std::deque<PhysicalPositionSample> physical_position_history_{};
  bool last_emitted_had_motion_{false};
};

}  // namespace sim

#endif
