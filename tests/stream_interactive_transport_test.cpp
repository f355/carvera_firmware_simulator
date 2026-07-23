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

#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "carvera_sim.pb.h"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"
#include "support/stream_stdio_harness.hpp"
#include "support/temp_sdcard.hpp"
#include "support/xmodem.hpp"

using sim::test::expect;

namespace {

bool timestamp_has_iso8601_shape(std::string_view output, std::size_t timestamp) {
  return output.size() >= timestamp + 24 && std::isdigit(static_cast<unsigned char>(output[timestamp + 0])) != 0 &&
         std::isdigit(static_cast<unsigned char>(output[timestamp + 1])) != 0 &&
         std::isdigit(static_cast<unsigned char>(output[timestamp + 2])) != 0 &&
         std::isdigit(static_cast<unsigned char>(output[timestamp + 3])) != 0 && output[timestamp + 4] == '-' &&
         output[timestamp + 7] == '-' && output[timestamp + 10] == 'T' && output[timestamp + 13] == ':' &&
         output[timestamp + 16] == ':' && output[timestamp + 19] == '.' && output[timestamp + 23] == 'Z';
}

bool has_iso8601_timestamped_tag(const std::string& output, std::string_view tag) {
  std::size_t line_start = 0;
  while (line_start < output.size()) {
    const auto line_end = output.find('\n', line_start);
    const auto line = std::string_view(output).substr(line_start, line_end - line_start);
    if (timestamp_has_iso8601_shape(line, 0) && line.size() > 25 && line[24] == ' ' &&
        line.substr(25, tag.size()) == tag) {
      return true;
    }
    if (line_end == std::string::npos) {
      break;
    }
    line_start = line_end + 1;
  }
  return false;
}

bool connect_controller_and_read_metadata(std::uint16_t port, int& client, std::string& metadata_output,
                                          std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    int candidate = -1;
    if (sim::test::connect_loopback(port, candidate, 1)) {
      const char metadata_query[] = "time\nversion\nmodel\n";
      if (sim::test::write_exact(candidate, metadata_query, sizeof(metadata_query) - 1)) {
        metadata_output = sim::test::read_until(candidate, "model =", std::chrono::seconds(1));
        if (metadata_output.find("model =") != std::string::npos) {
          client = candidate;
          return true;
        }
      }
      close(candidate);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  return false;
}

class DiscoveryListener {
 public:
  DiscoveryListener() {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
      return;
    }

    int yes = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(3333);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      close(fd_);
      fd_ = -1;
    }
  }

  ~DiscoveryListener() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  DiscoveryListener(const DiscoveryListener&) = delete;
  DiscoveryListener& operator=(const DiscoveryListener&) = delete;

  bool ok() const { return fd_ >= 0; }

  bool wait_for(std::string_view expected_name, std::uint16_t expected_port, std::chrono::milliseconds timeout) const {
    if (fd_ < 0) {
      return false;
    }

    const auto expected_payload = std::string(expected_name) + ",127.0.0.1," + std::to_string(expected_port) + ",0";
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(fd_, &read_set);
      timeval select_timeout{0, 100000};
      const int ready = select(fd_ + 1, &read_set, nullptr, nullptr, &select_timeout);
      if (ready > 0 && FD_ISSET(fd_, &read_set)) {
        char buffer[128]{};
        const auto received = recv(fd_, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
          const std::string beacon(buffer, static_cast<std::size_t>(received));
          if (beacon.find(expected_payload) != std::string::npos) {
            return true;
          }
        }
      }
    }

    return false;
  }

 private:
  int fd_{-1};
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stream_interactive_transport_test <carvera_sim_stream_stdio>\n";
    return 2;
  }

  sim::test::StreamStdioHarness simulator(argv[1]);
  if (!expect(simulator.start(), "failed to start stream simulator")) {
    return 1;
  }

  sim::test::TempSdCard sd("carvera_sim_stream_interactive_test");
  sim::test::CartesianConfigOptions config_options;
  config_options.protocol = sim::test::TestProtocol::Smoothie;
  config_options.extra = "spindle.delay_s 1\n";
  sim::test::write_cartesian_config(sd.path(), config_options);
  const std::string config_txt = "# Carvera simulator SD config.\n";
  sd.write_config_txt(config_txt);

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.set_id(100);
  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_C1);
  request.mutable_set_machine_model()->set_function_setting(4);
  if (!expect(simulator.request_ok(request, 100, response), "set_machine_model failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(1);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(simulator.request_ok(request, 1, response), "mount_filesystem failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(2);
  request.mutable_set_time_mode()->set_mode(carvera::sim::v1::TIME_MODE_REALTIME);
  if (!expect(simulator.request_ok(request, 2, response), "set_time_mode failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(3);
  request.mutable_get_machine_snapshot();
  if (!expect(simulator.request_ok(request, 3, response), "snapshot failed") ||
      !expect(response.machine_snapshot().homed(), "firmware should boot and home")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(4);
  request.mutable_start_interactive_transport()->set_enable_uart(true);
  request.mutable_start_interactive_transport()->set_log_traffic(true);
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  if (!expect(simulator.request_ok(request, 4, response), "start_interactive_transport failed") ||
      !expect(response.interactive_transport().tcp_endpoints_size() == 1, "expected one TCP endpoint")) {
    return 1;
  }

  int client = -1;
  const auto port = static_cast<std::uint16_t>(response.interactive_transport().tcp_endpoints(0).port());
  if (!expect(response.interactive_transport().tcp_endpoints(0).host() == "0.0.0.0",
              "TCP endpoint should listen on every IPv4 interface") ||
      !expect(port != 0, "TCP endpoint should expose a nonzero port")) {
    return 1;
  }
  DiscoveryListener discovery_listener;
  if (!expect(discovery_listener.ok(), "failed to listen for controller discovery UDP")) {
    return 1;
  }
  const bool discovered = discovery_listener.wait_for("CARVERA_01001", port, std::chrono::seconds(10));

  int stale_client = -1;
  const char stale_query[] = "version\n";
  if (!expect(sim::test::connect_loopback(port, stale_client),
              "stale controller should connect to stream-started TCP endpoint") ||
      !expect(sim::test::write_exact(stale_client, stale_query, sizeof(stale_query) - 1),
              "failed to write stale controller query")) {
    return 1;
  }
  close(stale_client);

  std::string metadata_output;
  if (!expect(connect_controller_and_read_metadata(port, client, metadata_output, std::chrono::seconds(5)),
              "controller should connect after a stale controller disconnected")) {
    return 1;
  }

  const char query[] = "?\n";
  if (!expect(sim::test::write_exact(client, query, sizeof(query) - 1),
              "failed to write status query to TCP endpoint")) {
    return 1;
  }

  const auto output = sim::test::read_until(client, "<");
  const char set_tool_command[] = "M493.2 T1\n";
  if (!expect(sim::test::write_exact(client, set_tool_command, sizeof(set_tool_command) - 1),
              "failed to write set-tool command to TCP endpoint")) {
    return 1;
  }
  (void)sim::test::read_until(client, "ok");

  const char spindle_on_command[] = "M3 S10000\n";
  if (!expect(sim::test::write_exact(client, spindle_on_command, sizeof(spindle_on_command) - 1),
              "failed to write spindle-on command to TCP endpoint")) {
    return 1;
  }
  std::vector<double> spindle_ramp_samples;
  bool saw_atc_state_in_stream = false;
  const bool spindle_ramp_seen = simulator.wait_until(
      [&](const std::vector<carvera::sim::v1::StreamFrame>& frames) {
        spindle_ramp_samples.clear();
        saw_atc_state_in_stream = false;
        for (const auto& frame : frames) {
          if (frame.payload_case() != carvera::sim::v1::StreamFrame::kEvent || !frame.event().has_machine_telemetry()) {
            continue;
          }
          const auto& telemetry = frame.event().machine_telemetry();
          saw_atc_state_in_stream = saw_atc_state_in_stream || telemetry.has_atc();
          const auto& spindle = telemetry.spindle();
          if (spindle.target_rpm() > 0.5 && (spindle.spinning() || spindle.actual_rpm() > 0.5)) {
            spindle_ramp_samples.push_back(spindle.actual_rpm());
          }
        }
        if (spindle_ramp_samples.size() < 5) {
          return false;
        }
        const auto [min_spindle_sample, max_spindle_sample] =
            std::minmax_element(spindle_ramp_samples.begin(), spindle_ramp_samples.end());
        return *max_spindle_sample - *min_spindle_sample > 1'000.0;
      },
      std::chrono::seconds(5));
  (void)sim::test::read_until(client, "ok");

  const char download_command[] = "download /sd/config.txt\n";
  if (!expect(sim::test::write_exact(client, download_command, sizeof(download_command) - 1),
              "failed to write download command to TCP endpoint")) {
    return 1;
  }
  // The controller starts XMODEM after the line command has reached the firmware.
  // Keeping the phases separate also prevents TCP from coalescing the first raw
  // handshake byte into the command parser's receive batch.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto downloaded_config = sim::test::receive_xmodem_download(client);
  close(client);
  simulator.stop();
  const auto stderr_output = simulator.stderr_output();

  if (!expect(output.find("<") != std::string::npos, "TCP endpoint should return firmware status while GUI is idle")) {
    return 1;
  }
  if (!expect(spindle_ramp_seen && spindle_ramp_samples.size() >= 5,
              "stream telemetry should include multiple spindle ramp samples during firmware dwell")) {
    return 1;
  }
  if (!expect(saw_atc_state_in_stream, "stream telemetry should include ATC physical state for live tool visuals")) {
    return 1;
  }
  const auto [min_spindle_sample, max_spindle_sample] =
      std::minmax_element(spindle_ramp_samples.begin(), spindle_ramp_samples.end());
  if (*max_spindle_sample - *min_spindle_sample <= 1'000.0) {
    std::cerr << "spindle samples:";
    for (const auto sample : spindle_ramp_samples) {
      std::cerr << ' ' << sample;
    }
    std::cerr << '\n';
  }
  if (!expect(spindle_ramp_seen && *max_spindle_sample - *min_spindle_sample > 1'000.0,
              "spindle ramp telemetry should expose gradual RPM changes instead of one final snapshot")) {
    return 1;
  }
  if (!expect(metadata_output.find("time = ") != std::string::npos,
              "TCP endpoint should return controller-readable firmware time")) {
    return 1;
  }
  if (!expect(metadata_output.find("version = 2.1.0c-simulator") != std::string::npos,
              "TCP endpoint should return controller-readable firmware version")) {
    return 1;
  }
  if (!expect(metadata_output.find("model = C1") != std::string::npos,
              "TCP endpoint should return controller-readable firmware model")) {
    return 1;
  }
  if (!expect(discovered, "stream-started TCP endpoint should advertise controller discovery UDP")) {
    return 1;
  }
  if (!expect(downloaded_config == config_txt, "TCP endpoint should XMODEM-download /sd/config.txt")) {
    return 1;
  }
  if (!expect(stderr_output.find("[sim wifi rx]") != std::string::npos,
              "interactive transport logging should include WiFi RX bytes")) {
    return 1;
  }
  if (!expect(has_iso8601_timestamped_tag(stderr_output, "[sim wifi rx]"),
              "interactive transport logging should timestamp WiFi RX bytes with ISO8601 UTC time")) {
    return 1;
  }
  if (!expect(stderr_output.find("[sim wifi tx]") != std::string::npos,
              "interactive transport logging should include WiFi TX bytes")) {
    return 1;
  }
  if (!expect(has_iso8601_timestamped_tag(stderr_output, "[sim transport]"),
              "interactive transport logging should include timestamped non-traffic transport events")) {
    return 1;
  }

  return 0;
}
