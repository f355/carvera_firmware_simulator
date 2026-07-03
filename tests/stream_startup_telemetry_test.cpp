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
#include <iostream>

#include "carvera_sim.pb.h"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/stream_stdio_harness.hpp"
#include "support/temp_sdcard.hpp"

using sim::test::expect;


int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_startup_telemetry_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  sim::test::StreamStdioHarness simulator(argv[1]);
  if (!expect(simulator.start(), "failed to start stream simulator")) {
    return 1;
  }

  sim::test::TempSdCard sd("carvera_sim_stream_startup_telemetry_test");
  sim::test::write_cartesian_config(sd.path());
  sd.write_config_txt("# Carvera simulator SD config.\n");

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
  request.mutable_start_interactive_transport()->set_enable_uart(false);
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  if (!expect(simulator.request_ok(request, 4, response), "start_interactive_transport failed") ||
      !expect(response.interactive_transport().tcp_endpoints_size() == 1, "expected one TCP endpoint")) {
    return 1;
  }

  const bool saw_machine_telemetry = simulator.wait_frame(
      [](const carvera::sim::v1::StreamFrame& frame) {
        if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_telemetry()) {
          return false;
        }
        const auto& telemetry = frame.event().machine_telemetry();
        return telemetry.firmware_booted() && telemetry.axes_size() > 0;
      },
      std::chrono::seconds(10));
  const bool saw_snapshot_with_work_area = simulator.wait_frame(
      [](const carvera::sim::v1::StreamFrame& frame) {
        if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_snapshot()) {
          return false;
        }
        const auto& snapshot = frame.event().machine_snapshot();
        return snapshot.firmware_booted() && snapshot.has_work_area();
      },
      std::chrono::seconds(10));

  return expect(saw_machine_telemetry, "interactive startup should emit firmware telemetry frames") &&
                 expect(saw_snapshot_with_work_area,
                        "interactive startup should emit full machine snapshots with soft-limit work area")
             ? 0
             : 1;
}
