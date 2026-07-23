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
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "sim/interactive_transport_manager.hpp"
#include "sim/simulation_instance.hpp"

namespace {

volatile std::sig_atomic_t keep_running = 1;

void on_signal(int) { keep_running = 0; }

void usage(const char* program) {
  std::cerr << "usage: " << program << " [--sd PATH] [--model c1|ca1] [--function-setting BYTE]\n"
            << "       [--wifi-port PORT]... [--no-uart] [--no-wifi]\n\n"
            << "If no --wifi-port is given, one localhost TCP port is opened on an ephemeral port.\n"
            << "PORT 0 also requests an ephemeral port.\n";
}

bool parse_u16(const std::string& text, std::uint16_t& value) {
  try {
    const auto parsed = std::stoul(text, nullptr, 0);
    if (parsed > 0xffff) {
      return false;
    }
    value = static_cast<std::uint16_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

bool parse_u8(const std::string& text, std::uint8_t& value) {
  std::uint16_t parsed = 0;
  if (!parse_u16(text, parsed) || parsed > 0xff) {
    return false;
  }
  value = static_cast<std::uint8_t>(parsed);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  bool enable_uart = true;
  bool enable_wifi = true;
  std::vector<std::uint16_t> wifi_ports;
  std::string sd_root;
  sim::FactorySettings factory;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      usage(argv[0]);
      return 0;
    }
    if (arg == "--no-uart") {
      enable_uart = false;
      continue;
    }
    if (arg == "--no-wifi") {
      enable_wifi = false;
      continue;
    }
    if (arg == "--sd" && i + 1 < argc) {
      sd_root = argv[++i];
      continue;
    }
    if (arg == "--wifi-port" && i + 1 < argc) {
      std::uint16_t port = 0;
      if (!parse_u16(argv[++i], port)) {
        std::cerr << "invalid --wifi-port\n";
        return 2;
      }
      wifi_ports.push_back(port);
      continue;
    }
    if (arg == "--model" && i + 1 < argc) {
      const std::string model = argv[++i];
      if (model == "c1" || model == "C1") {
        factory.machine_model = sim::MachineModel::CarveraC1;
      } else if (model == "ca1" || model == "CA1" || model == "air") {
        factory.machine_model = sim::MachineModel::CarveraAirCA1;
      } else {
        std::cerr << "invalid --model\n";
        return 2;
      }
      continue;
    }
    if (arg == "--function-setting" && i + 1 < argc) {
      if (!parse_u8(argv[++i], factory.function_setting)) {
        std::cerr << "invalid --function-setting\n";
        return 2;
      }
      continue;
    }

    usage(argv[0]);
    return 2;
  }

  if (enable_wifi && wifi_ports.empty()) {
    wifi_ports.push_back(0);
  }

  sim::PersistentMachineConfig persistent_config;
  if (!sd_root.empty()) {
    persistent_config.mounts.push_back({"sd", sd_root});
  }

  sim::SimulationInstance simulation(persistent_config);
  auto& runtime = simulation.firmware();
  if (!runtime.set_factory_settings(factory)) {
    std::cerr << "failed to configure factory settings\n";
    return 1;
  }

  sim::InteractiveTransportManager transports(runtime.io(), runtime.runner(), simulation.machine());
  carvera::sim::v1::StartInteractiveTransport start_transports;
  start_transports.set_enable_uart(enable_uart);
  if (enable_wifi) {
    for (const auto port : wifi_ports) {
      start_transports.add_tcp_ports(port);
    }
  }
  const auto start_result = transports.start(start_transports);
  if (!start_result.ok) {
    std::cerr << start_result.error << '\n';
    return 1;
  }
  if (enable_uart && start_result.transport.uart_path().empty()) {
    std::cerr << "virtual COM backend is not available on this platform\n";
    return 1;
  }

  if (!start_result.transport.uart_path().empty()) {
    std::cout << "UART " << start_result.transport.uart_path() << '\n';
  }
  for (const auto& endpoint : start_result.transport.tcp_endpoints()) {
    std::cout << "WIFI " << endpoint.host() << ':' << endpoint.port() << '\n';
  }
  std::cout.flush();

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  while (keep_running) {
    transports.pump();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  return 0;
}
