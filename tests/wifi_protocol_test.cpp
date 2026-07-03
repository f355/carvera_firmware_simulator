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

#include <cstdlib>
#include <iostream>
#include <string>

#include "sim/m8266_wifi.hpp"
#include "sim/simulation_instance.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::SimulationInstance simulation;
  auto& runtime = simulation.firmware();
  runtime.boot();

  auto& wifi = sim::m8266_wifi::active();
  require(wifi.tcp_server_port() == 2222, "real WifiProvider should configure the simulated M8266 TCP server");
  require(wifi.udp_listen_port() == 4444, "real WifiProvider should configure the simulated M8266 UDP listener");

  wifi.connect_tcp_client();
  runtime.write_wifi_tcp("?\n");
  std::string response;
  for (int i = 0; i < 20 && response.empty(); ++i) {
    runtime.pump_free_running();
    response += runtime.read_wifi_tcp();
  }

  require(response.find("<") != std::string::npos, "WiFi TCP should return firmware status through M8266 send APIs");
  require(response.find("MPos:") != std::string::npos, "WiFi TCP status should come from the real firmware query path");

  return 0;
}
