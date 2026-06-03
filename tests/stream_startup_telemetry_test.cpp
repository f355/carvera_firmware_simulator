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

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>

#include "carvera_sim.pb.h"
#include "support/cartesian_config.hpp"
#include "support/framed_proto_client.hpp"
#include "support/temp_sdcard.hpp"

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_startup_telemetry_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  int to_child[2]{};
  int from_child[2]{};
  if (pipe(to_child) != 0 || pipe(from_child) != 0) {
    std::cerr << "pipe failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  const auto child = fork();
  if (child < 0) {
    std::cerr << "fork failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  if (child == 0) {
    dup2(to_child[0], STDIN_FILENO);
    dup2(from_child[1], STDOUT_FILENO);
    close(to_child[0]);
    close(to_child[1]);
    close(from_child[0]);
    close(from_child[1]);
    execl(argv[1], argv[1], nullptr);
    _exit(127);
  }

  close(to_child[0]);
  close(from_child[1]);

  sim::test::TempSdCard sd("carvera_sim_stream_startup_telemetry_test");
  sim::test::write_cartesian_config(sd.path());
  sd.write_config_txt("# Carvera simulator SD config.\n");

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.set_id(1);
  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_C1);
  request.mutable_set_machine_model()->set_function_setting(4);
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write model request") ||
      !expect(sim::test::read_stream_response(from_child[0], 1, response), "failed to read model response") ||
      !expect(response.ok(), "set_machine_model failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(2);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write mount request") ||
      !expect(sim::test::read_stream_response(from_child[0], 2, response), "failed to read mount response") ||
      !expect(response.ok(), "mount_filesystem failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(3);
  request.mutable_set_time_mode()->set_mode(carvera::sim::v1::TIME_MODE_REALTIME);
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write realtime request") ||
      !expect(sim::test::read_stream_response(from_child[0], 3, response), "failed to read realtime response") ||
      !expect(response.ok(), "set_time_mode failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(4);
  request.mutable_start_interactive_transport()->set_enable_uart(false);
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write transport request") ||
      !expect(sim::test::read_stream_response(from_child[0], 4, response), "failed to read transport response") ||
      !expect(response.ok(), "start_interactive_transport failed") ||
      !expect(response.interactive_transport().tcp_endpoints_size() == 1, "expected one TCP endpoint")) {
    return 1;
  }

  bool saw_machine_telemetry = false;
  bool saw_snapshot_with_work_area = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline && !(saw_machine_telemetry && saw_snapshot_with_work_area)) {
    carvera::sim::v1::StreamFrame frame;
    if (!sim::test::read_stream_frame_timeout(from_child[0], frame, std::chrono::milliseconds(250))) {
      continue;
    }
    if (frame.payload_case() == carvera::sim::v1::StreamFrame::kEvent && frame.event().has_machine_telemetry()) {
      const auto& telemetry = frame.event().machine_telemetry();
      saw_machine_telemetry = telemetry.firmware_booted() && telemetry.axes_size() > 0;
    }
    if (frame.payload_case() == carvera::sim::v1::StreamFrame::kEvent && frame.event().has_machine_snapshot()) {
      const auto& snapshot = frame.event().machine_snapshot();
      saw_snapshot_with_work_area = snapshot.firmware_booted() && snapshot.has_work_area();
    }
  }

  close(to_child[1]);
  close(from_child[0]);
  int status = 0;
  waitpid(child, &status, 0);

  return expect(saw_machine_telemetry, "interactive startup should emit firmware telemetry frames") &&
                 expect(saw_snapshot_with_work_area,
                        "interactive startup should emit full machine snapshots with soft-limit work area")
             ? 0
             : 1;
}
