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

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "carvera_sim.pb.h"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"
#include "support/stream_stdio_harness.hpp"
#include "support/temp_sdcard.hpp"

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

bool telemetry_x_below(const carvera::sim::v1::StreamFrame& frame, double threshold) {
  if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_telemetry()) {
    return false;
  }
  for (const auto& axis : frame.event().machine_telemetry().axes()) {
    if (axis.axis() == carvera::sim::v1::AXIS_X && axis.physical_mm() < threshold) {
      return true;
    }
  }
  return false;
}

template <typename Predicate>
std::chrono::steady_clock::duration wait_timed(sim::test::StreamStdioHarness& simulator, Predicate&& predicate,
                                               std::chrono::milliseconds timeout) {
  const auto started = std::chrono::steady_clock::now();
  if (!simulator.wait_frame(std::forward<Predicate>(predicate), timeout)) {
    return std::chrono::steady_clock::duration::zero();
  }
  return std::chrono::steady_clock::now() - started;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_player_realtime_speed_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  sim::test::StreamStdioHarness simulator(argv[1]);
  if (!expect(simulator.start(), "failed to start stream simulator")) {
    return 1;
  }

  sim::test::TempSdCard sd("carvera_sim_stream_player_realtime_speed_test");
  sim::test::CartesianConfigOptions config_options;
  sim::test::write_cartesian_config(sd.path(), config_options);
  std::string job = "G91\n";
  for (int i = 0; i < 260; ++i) {
    job += "G1 X-1 Y-0.692 F3000\n";
  }
  sd.write("gcodes/speedtest.cnc", job);

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
  const int controller_flags = fcntl(controller, F_GETFL, 0);
  if (controller_flags >= 0) {
    fcntl(controller, F_SETFL, controller_flags | O_NONBLOCK);
  }

  const char start_job[] = "M23 speedtest\nM24\n";
  if (!expect(sim::test::write_exact(controller, start_job, sizeof(start_job) - 1), "failed to start Player job")) {
    return 1;
  }

  std::atomic_bool polling{true};
  std::thread status_poller([&] {
    while (polling.load()) {
      static constexpr char status_query[] = "?";
      (void)::write(controller, status_query, sizeof(status_query) - 1);
      (void)sim::test::read_available(controller, 2048);
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
  auto stop_polling = [&] {
    polling.store(false);
    if (status_poller.joinable()) {
      status_poller.join();
    }
  };

  const auto baseline_elapsed = wait_timed(
      simulator, [](const auto& frame) { return telemetry_x_below(frame, -50.0); }, std::chrono::seconds(12));
  if (!expect(baseline_elapsed != std::chrono::steady_clock::duration::zero(),
              "Player job should move through the baseline segment at 1x")) {
    stop_polling();
    return 1;
  }

  request.Clear();
  response.Clear();
  request.mutable_set_realtime_speed()->set_multiplier(10.0);
  if (!expect(simulator.request_ok(request, 6, response), "set_realtime_speed during Player job failed")) {
    stop_polling();
    return 1;
  }
  const auto accelerated_elapsed = wait_timed(
      simulator, [](const auto& frame) { return telemetry_x_below(frame, -100.0); }, std::chrono::seconds(4));
  std::cerr << "baseline_elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(baseline_elapsed).count()
            << " accelerated_elapsed_ms="
            << std::chrono::duration_cast<std::chrono::milliseconds>(accelerated_elapsed).count() << '\n';
  if (!expect(accelerated_elapsed != std::chrono::steady_clock::duration::zero(),
              "Player job should complete the accelerated segment")) {
    stop_polling();
    return 1;
  }
  if (!expect(accelerated_elapsed * 3 < baseline_elapsed * 2,
              "accelerated Player segment should be observably faster than the 1x segment")) {
    std::cerr << simulator.stderr_output() << '\n';
    stop_polling();
    return 1;
  }

  stop_polling();
  close(controller);
  return 0;
}
