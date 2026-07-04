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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/framed_proto.hpp"
#include "sim/logging.hpp"
#include "sim/machine_state_proto.hpp"
#include "sim/motion_telemetry.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/simulator_context.hpp"
#include "sim/stream_request_pump.hpp"

namespace {

class PhysicalIoEventEmitter {
 public:
  bool emit_due(sim::ApiService& api) {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_emit_) {
      return true;
    }
    next_emit_ = now + std::chrono::milliseconds(100);

    carvera::sim::v1::PhysicalIoSnapshot snapshot;
    if (!api.fill_physical_io_snapshot_nonblocking(snapshot)) {
      return true;
    }

    carvera::sim::v1::StreamFrame frame;
    auto* event = frame.mutable_event();
    event->set_sequence(next_sequence_++);
    event->mutable_physical_io()->CopyFrom(snapshot);
    if (!sim::proto_framing::write_message(std::cout, frame)) {
      return false;
    }
    std::cout.flush();
    return true;
  }

 private:
  std::chrono::steady_clock::time_point next_emit_{};
  std::uint64_t next_sequence_{1};
};

class MachineSnapshotEventEmitter {
 public:
  bool emit_due(sim::ApiService& api) {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_emit_) {
      return true;
    }
    next_emit_ = now + std::chrono::milliseconds(200);

    carvera::sim::v1::MachineSnapshot snapshot;
    if (!api.fill_machine_snapshot_nonblocking(snapshot)) {
      return true;
    }
    log_state_transitions(snapshot);

    carvera::sim::v1::StreamFrame frame;
    auto* event = frame.mutable_event();
    event->set_sequence(next_sequence_++);
    event->mutable_machine_snapshot()->CopyFrom(snapshot);
    if (!sim::proto_framing::write_message(std::cout, frame)) {
      return false;
    }
    std::cout.flush();
    return true;
  }

 private:
  void log_state_transitions(const carvera::sim::v1::MachineSnapshot& snapshot) {
    if (!last_booted_.has_value() || *last_booted_ != snapshot.firmware_booted()) {
      sim::logging::event("firmware", snapshot.firmware_booted() ? "booted" : "off");
      last_booted_ = snapshot.firmware_booted();
    }
    if (!last_homed_.has_value() || *last_homed_ != snapshot.homed()) {
      sim::logging::event("motion", snapshot.homed() ? "machine homed" : "machine not homed");
      last_homed_ = snapshot.homed();
    }
    std::vector<std::int64_t> axis_steps;
    axis_steps.reserve(static_cast<std::size_t>(snapshot.axes_size()));
    for (const auto& axis : snapshot.axes()) {
      axis_steps.push_back(axis.physical_steps());
    }
    if (last_axis_steps_.has_value()) {
      const bool axes_changed = axis_steps != *last_axis_steps_;
      if (axes_changed != axes_moving_) {
        sim::logging::event("motion", axes_changed ? "axes moving" : "axes idle");
        axes_moving_ = axes_changed;
      }
    }
    last_axis_steps_ = std::move(axis_steps);
  }

  std::chrono::steady_clock::time_point next_emit_{};
  std::uint64_t next_sequence_{1};
  std::optional<bool> last_booted_;
  std::optional<bool> last_homed_;
  std::optional<std::vector<std::int64_t>> last_axis_steps_;
  bool axes_moving_{false};
};

}  // namespace

int main() {
  sim::SimulationInstance simulation;
  sim::ApiService api(simulation);
  sim::StreamRequestPump stream_requests(std::cout);
  MachineSnapshotEventEmitter machine_snapshot_events;
  PhysicalIoEventEmitter physical_io_events;
  bool stream_ok = true;
  api.set_auxiliary_interactive_pump(
      [&api, &stream_requests, &machine_snapshot_events, &physical_io_events, &stream_ok] {
        stream_ok = stream_ok && stream_requests.drain_available([&api](const carvera::sim::v1::Request& request) {
          return api.handle_cooperative(request);
        });
        stream_ok = stream_ok && machine_snapshot_events.emit_due(api);
        stream_ok = stream_ok && physical_io_events.emit_due(api);
      });

  auto& telemetry = simulation.machine().context().motion_telemetry();
  telemetry.set_interval_ticks(1000);
  telemetry.set_sink([](const sim::MachineTelemetry& sample) {
    carvera::sim::v1::StreamFrame frame;
    auto* event = frame.mutable_event();
    event->set_sequence(sample.sequence);
    sim::api::fill_machine_telemetry_proto(*event->mutable_machine_telemetry(), sample);
    sim::proto_framing::write_message(std::cout, frame);
    std::cout.flush();
  });

  for (;;) {
    if (!stream_requests.drain_available(
            [&](const carvera::sim::v1::Request& request) { return api.handle(request); })) {
      break;
    }

    if (stream_requests.closed()) {
      break;
    }

    api.pump_interactive();
    if (!stream_ok) {
      break;
    }
    if (!machine_snapshot_events.emit_due(api)) {
      break;
    }
    if (!physical_io_events.emit_due(api)) {
      break;
    }
  }

  return stream_requests.closed() ? 0 : 1;
}
