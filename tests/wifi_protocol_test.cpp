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

#include <string>

#include "StreamOutput.h"
#include "sim/makera_protocol.hpp"
#include "sim/m8266_wifi.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/simulator_context.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

std::string require_payload(std::string bytes, sim::makera::PacketType type, const char* message) {
  sim::makera::FrameDecoder decoder;
  decoder.append(bytes);
  auto frames = decoder.take_frames();
  require(frames.size() == 1 && frames[0].type == type, message);
  return frames[0].payload;
}

}  // namespace

int main() {
  sim::SimulationInstance simulation;
  auto& runtime = simulation.firmware();
  runtime.boot();
  require(communication_protocol == PROTOCOL_MAKERA, "stock C1 firmware configuration should select Makera framing");

  auto& wifi = simulation.machine().context().m8266_wifi();
  require(wifi.tcp_server_port() == 2222, "real WifiProvider should configure the simulated M8266 TCP server");
  require(wifi.udp_listen_port() == 4444, "real WifiProvider should configure the simulated M8266 UDP listener");

  runtime.io().read_serial();
  runtime.io().write_serial(sim::makera::encode_console_input("?"));
  runtime.runner().run_main_loop(2);
  const auto serial_status = require_payload(runtime.io().read_serial(), sim::makera::PacketType::StatusResponse,
                                             "serial console should return one framed Makera status response");
  require(serial_status.find("MPos:") != std::string::npos,
          "serial Makera status should come from the real firmware query path");

  wifi.connect_tcp_client();
  runtime.io().write_wifi_tcp(sim::makera::encode_console_input("?"));
  std::string response;
  for (int i = 0; i < 20 && response.empty(); ++i) {
    runtime.runner().run_main_loop(1);
    response += runtime.io().read_wifi_tcp();
  }

  require(!wifi.has_received_data(), "WifiProvider should consume the Makera request frame");
  const auto status = require_payload(std::move(response), sim::makera::PacketType::StatusResponse,
                                      "WiFi should return one framed Makera status response");
  require(status.find("<") != std::string::npos, "WiFi TCP should return firmware status through M8266 send APIs");
  require(status.find("MPos:") != std::string::npos, "WiFi TCP status should come from the real firmware query path");

  return 0;
}
