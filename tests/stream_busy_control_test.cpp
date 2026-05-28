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

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>

#include "carvera_sim.pb.h"
#include "support/cartesian_config.hpp"
#include "support/framed_proto_client.hpp"
#include "support/posix_io.hpp"
#include "support/temp_sdcard.hpp"

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

bool request_ok(int to_child, int from_child, std::uint64_t id, carvera::sim::v1::Request& request,
                carvera::sim::v1::Response& response) {
  request.set_id(id);
  return sim::test::write_framed_message(to_child, request) &&
         sim::test::read_stream_response(from_child, id, response) && response.ok();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_busy_control_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  int to_child[2]{};
  int from_child[2]{};
  int err_child[2]{};
  if (pipe(to_child) != 0 || pipe(from_child) != 0 || pipe(err_child) != 0) {
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
    dup2(err_child[1], STDERR_FILENO);
    close(to_child[0]);
    close(to_child[1]);
    close(from_child[0]);
    close(from_child[1]);
    close(err_child[0]);
    close(err_child[1]);
    execl(argv[1], argv[1], nullptr);
    _exit(127);
  }

  close(to_child[0]);
  close(from_child[1]);
  close(err_child[1]);

  sim::test::TempSdCard sd("carvera_sim_stream_busy_control_test");
  sim::test::CartesianConfigOptions config_options;
  sim::test::write_cartesian_config(sd.path(), config_options);

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_C1);
  request.mutable_set_machine_model()->set_function_setting(4);
  if (!expect(request_ok(to_child[1], from_child[0], 1, request, response), "set_machine_model failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(request_ok(to_child[1], from_child[0], 2, request, response), "mount_filesystem failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_set_time_mode()->set_mode(carvera::sim::v1::TIME_MODE_REALTIME);
  if (!expect(request_ok(to_child[1], from_child[0], 3, request, response), "set_time_mode failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_get_machine_snapshot();
  if (!expect(request_ok(to_child[1], from_child[0], 4, request, response), "initial snapshot failed") ||
      !expect(response.machine_snapshot().homed(), "firmware should boot and home")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  if (!expect(request_ok(to_child[1], from_child[0], 5, request, response), "start_interactive_transport failed") ||
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

  bool firmware_busy = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline && !firmware_busy) {
    carvera::sim::v1::StreamFrame frame;
    if (!sim::test::read_stream_frame_timeout(from_child[0], frame, std::chrono::milliseconds(250))) {
      continue;
    }
    if (frame.payload_case() == carvera::sim::v1::StreamFrame::kEvent && frame.event().has_machine_telemetry()) {
      for (const auto& axis : frame.event().machine_telemetry().axes()) {
        if (axis.axis() == carvera::sim::v1::AXIS_Z && axis.machine_position() < -3.0) {
          firmware_busy = true;
        }
      }
    }
  }
  if (!expect(firmware_busy, "probing move should produce Z telemetry while firmware is busy")) {
    return 1;
  }

  request.Clear();
  request.set_id(20);
  request.mutable_get_machine_snapshot();
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write stale snapshot request")) {
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
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write stock geometry request")) {
    return 1;
  }

  request.Clear();
  request.set_id(22);
  request.mutable_get_eeprom_fields();
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write EEPROM fields request")) {
    return 1;
  }

  request.Clear();
  request.set_id(23);
  request.mutable_set_e_stop_pressed()->set_pressed(true);
  if (!expect(sim::test::write_framed_message(to_child[1], request), "failed to write e-stop request")) {
    return 1;
  }

  response.Clear();
  const bool stale_snapshot_rejected =
      sim::test::read_stream_response_timeout(from_child[0], 20, response, std::chrono::milliseconds(500)) &&
      !response.ok();
  if (!expect(stale_snapshot_rejected, "busy stream should reject non-urgent snapshot reads quickly")) {
    return 1;
  }

  response.Clear();
  const bool stock_acknowledged =
      sim::test::read_stream_response_timeout(from_child[0], 21, response, std::chrono::milliseconds(500)) &&
      response.ok();
  if (!expect(stock_acknowledged, "busy stream should acknowledge physical stock geometry quickly")) {
    return 1;
  }

  response.Clear();
  const bool eeprom_acknowledged =
      sim::test::read_stream_response_timeout(from_child[0], 22, response, std::chrono::milliseconds(500)) &&
      response.ok() && response.has_eeprom_fields();
  if (!expect(eeprom_acknowledged, "busy stream should acknowledge EEPROM hardware reads quickly")) {
    return 1;
  }

  response.Clear();
  const bool e_stop_acknowledged =
      sim::test::read_stream_response_timeout(from_child[0], 23, response, std::chrono::milliseconds(500)) &&
      response.ok();
  if (!expect(e_stop_acknowledged, "busy stream should acknowledge urgent e-stop input quickly")) {
    return 1;
  }

  close(controller);
  close(to_child[1]);
  close(from_child[0]);
  close(err_child[0]);
  kill(child, SIGTERM);
  int status = 0;
  waitpid(child, &status, 0);
  return 0;
}
