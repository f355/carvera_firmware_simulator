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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "sim/firmware_runtime.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/interactive_io.hpp"
#include "sim/machine_simulator.hpp"
#include "support/cartesian_config.hpp"
#include "support/posix_io.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
#ifdef _WIN32
  return 0;
#else
  const auto root = std::filesystem::temp_directory_path() / "carvera_sim_interactive_io_test";
  std::filesystem::remove_all(root);
  sim::test::write_cartesian_config(root);
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime runtime(simulator);
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
  for (int i = 0; i < 100 && (serial_output.find("ok") == std::string::npos ||
                              simulator.axis_position_steps(0) == initial_serial_x_steps);
       ++i) {
    uart.poll();
    runtime.pump_free_running();
    uart.poll();
    serial_output += sim::test::read_available(serial_fd, 256);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  require(serial_output.find("ok") != std::string::npos, "virtual COM bridge should return firmware serial output");
  require(simulator.axis_position_steps(0) != initial_serial_x_steps,
          "virtual COM bridge should accept controller homing and jog commands");
  ::close(serial_fd);

  sim::MachineSimulator tcp_simulator;
  sim::FirmwareRuntime tcp_runtime(tcp_simulator);
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
  for (int i = 0; i < 100 && (tcp_output.find("ok") == std::string::npos ||
                              tcp_simulator.axis_position_steps(0) == initial_tcp_x_steps);
       ++i) {
    wifi.poll();
    tcp_runtime.pump_free_running();
    wifi.poll();
    tcp_output += sim::test::read_available(client, 256);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  require(tcp_output.find("ok") != std::string::npos, "localhost WiFi bridge should return firmware serial output");
  require(tcp_simulator.axis_position_steps(0) != initial_tcp_x_steps,
          "localhost WiFi bridge should accept controller homing and jog commands");
  ::close(client);

  sim::MachineSimulator backlog_simulator;
  sim::FirmwareRuntime backlog_runtime(backlog_simulator);
  backlog_runtime.boot();
  sim::LocalhostTcpBridge backlog_wifi(backlog_runtime);
  require(backlog_wifi.start(0), "backlog WiFi bridge should start");

  int backlog_client = -1;
  require(sim::test::connect_loopback(backlog_wifi.port(), backlog_client), "backlog TCP client should connect");
  require(sim::test::set_nonblocking(backlog_client), "backlog TCP client should use nonblocking reads");
  int small_receive_buffer = 4096;
  ::setsockopt(backlog_client, SOL_SOCKET, SO_RCVBUF, &small_receive_buffer, sizeof(small_receive_buffer));

  for (int i = 0; i < 50; ++i) {
    backlog_wifi.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  const std::string large_payload(256 * 1024, 'x');
  backlog_wifi.write_output(large_payload);
  require(backlog_wifi.queued_output_bytes() > 0,
          "localhost WiFi bridge should keep burst backpressure in its own output queue");

  std::string backlog_output;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (backlog_output.size() < large_payload.size() && std::chrono::steady_clock::now() < deadline) {
    backlog_wifi.poll();
    backlog_output += sim::test::read_available(backlog_client, 16 * 1024);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  require(backlog_output.size() == large_payload.size(),
          "localhost WiFi bridge should queue large outbound bursts until the controller drains them");
  ::close(backlog_client);

  std::filesystem::remove_all(root);
  return 0;
#endif
}
