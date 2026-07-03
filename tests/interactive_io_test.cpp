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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#include "sim/interactive_io.hpp"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"

#include <unistd.h>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template <typename Predicate, typename Pump>
bool wait_pumping(Predicate&& predicate, Pump&& pump, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    pump();
  }
  return predicate();
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_interactive_io_test");
  const auto& root = temp_root.path();
  sim::test::write_cartesian_config(root);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& simulator = simulation.machine();
  auto& runtime = simulation.firmware();
  runtime.boot();

  sim::VirtualComPort uart(runtime);
  require(uart.start(), "virtual COM port should start on POSIX");
  require(!uart.device_path().empty(), "virtual COM port should expose a device path");
  const auto initial_serial_x_steps = simulator.axis_position_steps(0);

  const int serial_fd = sim::test::open_virtual_com_slave(uart.device_path());
  require(serial_fd >= 0, "virtual COM slave path should open");
  const char serial_jog[] = "$H\n$J=G91 X1 F1500\n";
  require(::write(serial_fd, serial_jog, std::strlen(serial_jog)) == static_cast<ssize_t>(std::strlen(serial_jog)),
          "virtual COM write should succeed");

  std::string serial_output;
  auto pump_serial = [&] {
    uart.poll();
    runtime.pump_free_running();
    uart.poll();
    serial_output += sim::test::read_available(serial_fd, 256);
  };
  wait_pumping(
      [&] {
        return serial_output.find("ok") != std::string::npos &&
               simulator.axis_position_steps(0) != initial_serial_x_steps;
      },
      pump_serial);
  require(serial_output.find("ok") != std::string::npos, "virtual COM bridge should return firmware serial output");
  require(simulator.axis_position_steps(0) != initial_serial_x_steps,
          "virtual COM bridge should accept controller homing and jog commands");
  ::close(serial_fd);

  sim::SimulationInstance tcp_simulation;
  auto& tcp_simulator = tcp_simulation.machine();
  auto& tcp_runtime = tcp_simulation.firmware();
  tcp_runtime.boot();
  sim::LocalhostTcpBridge wifi(tcp_runtime);
  require(wifi.start(0), "localhost WiFi bridge should start on an ephemeral port");
  require(wifi.port() != 0, "localhost WiFi bridge should report the bound port");
  const auto initial_tcp_x_steps = tcp_simulator.axis_position_steps(0);

  int client = -1;
  require(sim::test::connect_loopback(wifi.port(), client), "TCP client should connect to localhost WiFi bridge");
  const char tcp_jog[] = "$H\n$J=G91 X1 F1500\n";
  require(::write(client, tcp_jog, std::strlen(tcp_jog)) == static_cast<ssize_t>(std::strlen(tcp_jog)),
          "TCP client write should succeed");
  require(sim::test::set_nonblocking(client), "TCP client should switch to nonblocking reads");

  std::string tcp_output;
  auto pump_tcp = [&] {
    wifi.poll();
    tcp_runtime.pump_free_running();
    wifi.poll();
    tcp_output += sim::test::read_available(client, 256);
  };
  wait_pumping(
      [&] {
        return tcp_output.find("ok") != std::string::npos &&
               tcp_simulator.axis_position_steps(0) != initial_tcp_x_steps;
      },
      pump_tcp);
  require(tcp_output.find("ok") != std::string::npos, "localhost WiFi bridge should return firmware serial output");
  require(tcp_simulator.axis_position_steps(0) != initial_tcp_x_steps,
          "localhost WiFi bridge should accept controller homing and jog commands");
  ::close(client);

  sim::SimulationInstance backlog_simulation;
  auto& backlog_runtime = backlog_simulation.firmware();
  backlog_runtime.boot();
  sim::LocalhostTcpBridge backlog_wifi(backlog_runtime);
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
