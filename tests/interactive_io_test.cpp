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
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

#include "sim/interactive_io.hpp"
#include "sim/makera_protocol.hpp"
#include "sim/simulation_instance.hpp"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"
#include "support/temp_sdcard.hpp"

#include <unistd.h>
#include "StreamOutput.h"
#include "support/assertions.hpp"
#include "support/waiting.hpp"

using sim::test::require;
using sim::test::wait_pumping;

namespace {

std::string decode_payloads(sim::makera::FrameDecoder& decoder, std::string bytes) {
  decoder.append(bytes);
  std::string output;
  for (auto& frame : decoder.take_frames()) {
    output += frame.payload;
  }
  return output;
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_interactive_io_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.protocol = sim::test::TestProtocol::Makera;
  sim::test::write_cartesian_config(root, config);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  runtime.boot();
  require(communication_protocol == PROTOCOL_MAKERA, "virtual COM test should boot with Makera protocol");

  sim::VirtualComPort uart(runtime.io());
  require(uart.start(), "virtual COM port should start on POSIX");
  require(!uart.device_path().empty(), "virtual COM port should expose a device path");
  const auto initial_serial_x_steps = simulator.axis_position_steps(0);

  const int serial_fd = sim::test::open_virtual_com_slave(uart.device_path());
  require(serial_fd >= 0, "virtual COM slave path should open");
  const auto serial_jog = sim::makera::encode_console_input("$J=G91 X1 F1500\n");
  require(::write(serial_fd, serial_jog.data(), serial_jog.size()) == static_cast<ssize_t>(serial_jog.size()),
          "virtual COM write should succeed");

  sim::makera::FrameDecoder serial_decoder;
  std::string serial_output;
  auto pump_serial = [&] {
    uart.poll();
    runtime.runner().run_until_motion_idle(200'000);
    uart.poll();
    serial_output += decode_payloads(serial_decoder, sim::test::read_available(serial_fd, 256));
  };
  wait_pumping(
      [&] {
        return serial_output.find("ok") != std::string::npos &&
               simulator.axis_position_steps(0) != initial_serial_x_steps;
      },
      pump_serial);
  require(serial_output.find("ok") != std::string::npos, "virtual COM bridge should return firmware serial output");
  require(simulator.axis_position_steps(0) != initial_serial_x_steps,
          "virtual COM bridge should accept controller jog commands");
  ::close(serial_fd);
  uart.stop();

  sim::LocalhostTcpBridge wifi(runtime.io());
  require(wifi.start(0), "localhost WiFi bridge should start on an ephemeral port");
  require(wifi.port() != 0, "localhost WiFi bridge should report the bound port");

  const auto non_loopback_address = sim::test::non_loopback_ipv4_address();
  require(!non_loopback_address.empty(), "test host should expose a non-loopback IPv4 address");
  int client = -1;
  require(sim::test::connect_ipv4(non_loopback_address.c_str(), wifi.port(), client),
          "TCP client should connect to the WiFi bridge through any local IPv4 address");
  const auto tcp_query = sim::makera::encode_console_input("?");
  require(::write(client, tcp_query.data(), tcp_query.size()) == static_cast<ssize_t>(tcp_query.size()),
          "TCP client write should succeed");
  require(sim::test::set_nonblocking(client), "TCP client should switch to nonblocking reads");

  sim::makera::FrameDecoder tcp_decoder;
  std::string tcp_output;
  auto pump_tcp = [&] {
    wifi.poll();
    runtime.runner().run_until_motion_idle(200'000);
    wifi.poll();
    tcp_output += decode_payloads(tcp_decoder, sim::test::read_available(client, 256));
  };
  wait_pumping([&] { return tcp_output.find("MPos:") != std::string::npos; }, pump_tcp);
  require(tcp_output.find("MPos:") != std::string::npos,
          "localhost WiFi bridge should return a framed firmware status response");
  ::close(client);
  wifi.stop();

  sim::LocalhostTcpBridge backlog_wifi(runtime.io());
  require(backlog_wifi.start(0), "backlog WiFi bridge should start");

  int backlog_client = -1;
  require(sim::test::connect_loopback(backlog_wifi.port(), backlog_client), "backlog TCP client should connect");
  require(sim::test::set_nonblocking(backlog_client), "backlog TCP client should use nonblocking reads");
  int small_receive_buffer = 4096;
  ::setsockopt(backlog_client, SOL_SOCKET, SO_RCVBUF, &small_receive_buffer, sizeof(small_receive_buffer));

  std::string backlog_output;
  const std::string ready_marker = "ready";
  auto pump_backlog = [&] {
    backlog_wifi.poll();
    backlog_output += sim::test::read_available(backlog_client, 16 * 1024);
  };
  auto pump_backlog_until_ready = [&] {
    backlog_wifi.poll();
    backlog_wifi.write_output(ready_marker);
    backlog_output += sim::test::read_available(backlog_client, 16 * 1024);
  };
  wait_pumping([&] { return backlog_output.find(ready_marker) != std::string::npos; }, pump_backlog_until_ready,
               std::chrono::seconds(5));
  require(backlog_output.find(ready_marker) != std::string::npos,
          "backlog TCP client should receive a marker before burst testing starts");
  backlog_output.clear();

  const std::string large_payload(256 * 1024, 'x');
  backlog_wifi.write_output(large_payload);

  wait_pumping(
      [&] {
        return static_cast<std::size_t>(std::count(backlog_output.begin(), backlog_output.end(), 'x')) >=
               large_payload.size();
      },
      pump_backlog, std::chrono::seconds(10));
  require(
      static_cast<std::size_t>(std::count(backlog_output.begin(), backlog_output.end(), 'x')) == large_payload.size(),
      "localhost WiFi bridge should queue large outbound bursts until the controller drains them");
  ::close(backlog_client);
  return 0;
}
