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
#include <cstdint>
#include <iostream>

#include "carvera_sim.pb.h"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"
#include "support/stream_stdio_harness.hpp"
#include "support/temp_sdcard.hpp"

using sim::test::expect;

namespace {

constexpr auto kBusyProbeDeadline = std::chrono::seconds(5);
constexpr auto kBusyResponseDeadline = std::chrono::seconds(2);
constexpr auto kAcceleratedMotionDeadline = std::chrono::milliseconds(1500);

bool telemetry_z_below(const carvera::sim::v1::StreamFrame& frame, double threshold) {
  if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_telemetry()) {
    return false;
  }
  for (const auto& axis : frame.event().machine_telemetry().axes()) {
    if (axis.axis() == carvera::sim::v1::AXIS_Z && axis.machine_position() < threshold) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_busy_control_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  sim::test::StreamStdioHarness simulator(argv[1]);
  if (!expect(simulator.start(), "failed to start stream simulator")) {
    return 1;
  }

  sim::test::TempSdCard sd("carvera_sim_stream_busy_control_test");
  sim::test::CartesianConfigOptions config_options;
  sim::test::write_cartesian_config(sd.path(), config_options);

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_C1);
  request.mutable_set_machine_model()->set_function_setting(4);
  if (!expect(simulator.request_ok(request, 1, response), "set_machine_model failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(simulator.request_ok(request, 2, response), "mount_filesystem failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_set_time_mode()->set_mode(carvera::sim::v1::TIME_MODE_REALTIME);
  if (!expect(simulator.request_ok(request, 3, response), "set_time_mode failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_get_machine_snapshot();
  if (!expect(simulator.request_ok(request, 4, response), "initial snapshot failed") ||
      !expect(response.machine_snapshot().homed(), "firmware should boot and home")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  if (!expect(simulator.request_ok(request, 5, response), "start_interactive_transport failed") ||
      !expect(response.interactive_transport().tcp_endpoints_size() == 1, "expected one TCP endpoint")) {
    return 1;
  }

  int controller = -1;
  const auto port = static_cast<std::uint16_t>(response.interactive_transport().tcp_endpoints(0).port());
  if (!expect(sim::test::connect_loopback(port, controller), "controller should connect to TCP endpoint")) {
    return 1;
  }

  const char probing_command[] = "G38.6 Z-118 F500\n";
  if (!expect(sim::test::write_exact(controller, probing_command, sizeof(probing_command) - 1),
              "failed to write probing command")) {
    return 1;
  }

  const bool firmware_busy = simulator.wait_frame(
      [](const carvera::sim::v1::StreamFrame& frame) {
        if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_telemetry()) {
          return false;
        }
        for (const auto& axis : frame.event().machine_telemetry().axes()) {
          if (axis.axis() == carvera::sim::v1::AXIS_Z && axis.machine_position() < -3.0) {
            return true;
          }
        }
        return false;
      },
      kBusyProbeDeadline);
  if (!expect(firmware_busy, "probing move should produce Z telemetry while firmware is busy")) {
    return 1;
  }

  request.Clear();
  request.set_id(20);
  request.mutable_get_machine_snapshot();
  if (!expect(simulator.write_request(request), "failed to write stale snapshot request")) {
    return 1;
  }

  request.Clear();
  request.set_id(21);
  auto* stock = request.mutable_set_stock_box();
  stock->set_enabled(true);
  stock->mutable_bounds()->set_min_x(-10.0);
  stock->mutable_bounds()->set_min_y(-10.0);
  stock->mutable_bounds()->set_min_z(-5.0);
  stock->mutable_bounds()->set_max_x(10.0);
  stock->mutable_bounds()->set_max_y(10.0);
  stock->mutable_bounds()->set_max_z(-1.0);
  if (!expect(simulator.write_request(request), "failed to write stock geometry request")) {
    return 1;
  }

  request.Clear();
  request.set_id(22);
  request.mutable_get_eeprom_contents();
  if (!expect(simulator.write_request(request), "failed to write EEPROM contents request")) {
    return 1;
  }

  request.Clear();
  request.set_id(23);
  request.mutable_set_realtime_speed()->set_multiplier(4.0);
  if (!expect(simulator.write_request(request), "failed to write realtime speed request")) {
    return 1;
  }

  response.Clear();
  const bool stale_snapshot_rejected = simulator.wait_response(20, response, kBusyResponseDeadline) && !response.ok();
  if (!expect(stale_snapshot_rejected, "busy stream should reject non-urgent snapshot reads quickly")) {
    return 1;
  }

  response.Clear();
  const bool stock_acknowledged = simulator.wait_response(21, response, kBusyResponseDeadline) && response.ok();
  if (!expect(stock_acknowledged, "busy stream should acknowledge physical stock geometry quickly")) {
    return 1;
  }

  response.Clear();
  const bool eeprom_acknowledged =
      simulator.wait_response(22, response, kBusyResponseDeadline) && response.ok() && response.has_eeprom_contents();
  if (!expect(eeprom_acknowledged, "busy stream should acknowledge EEPROM hardware reads quickly")) {
    return 1;
  }

  response.Clear();
  const bool realtime_speed_acknowledged =
      simulator.wait_response(23, response, kBusyResponseDeadline) && response.ok();
  if (!expect(realtime_speed_acknowledged, "busy stream should acknowledge realtime speed changes quickly")) {
    return 1;
  }

  if (!expect(
          simulator.wait_frame([](const auto& frame) { return telemetry_z_below(frame, -20.0); }, kBusyProbeDeadline),
          "probing move should continue after realtime speed change")) {
    return 1;
  }

  const auto accelerated_start = std::chrono::steady_clock::now();
  if (!expect(simulator.wait_frame([](const auto& frame) { return telemetry_z_below(frame, -40.0); },
                                   kAcceleratedMotionDeadline),
              "busy stream realtime speed change should accelerate subsequent motion")) {
    return 1;
  }
  const auto accelerated_elapsed = std::chrono::steady_clock::now() - accelerated_start;
  if (!expect(accelerated_elapsed < kAcceleratedMotionDeadline,
              "accelerated motion threshold should be reached before the deadline")) {
    return 1;
  }

  request.Clear();
  request.set_id(24);
  request.mutable_set_e_stop_pressed()->set_pressed(true);
  if (!expect(simulator.write_request(request), "failed to write e-stop request")) {
    return 1;
  }

  response.Clear();
  const bool e_stop_acknowledged = simulator.wait_response(24, response, kBusyResponseDeadline) && response.ok();
  if (!expect(e_stop_acknowledged, "busy stream should acknowledge urgent e-stop input quickly")) {
    return 1;
  }

  close(controller);
  return 0;
}
