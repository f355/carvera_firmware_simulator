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

#include <cstdint>
#include <string>
#include <string_view>

#include "PublicData.h"
#include "StreamOutput.h"
#include "sim/m8266_wifi.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/simulator_context.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

std::uint16_t crc16_ccitt(std::string_view data) {
  std::uint16_t crc = 0;
  for (const auto value : data) {
    crc ^= static_cast<std::uint16_t>(static_cast<unsigned char>(value)) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
  }
  return crc;
}

std::string makera_frame(std::uint8_t command, std::string_view payload) {
  const auto data_length = static_cast<std::uint16_t>(payload.size() + 3);
  std::string frame;
  frame.reserve(payload.size() + 9);
  frame.push_back(static_cast<char>(HEADER >> 8));
  frame.push_back(static_cast<char>(HEADER & 0xff));
  frame.push_back(static_cast<char>(data_length >> 8));
  frame.push_back(static_cast<char>(data_length & 0xff));
  frame.push_back(static_cast<char>(command));
  frame.append(payload);
  const auto crc = crc16_ccitt(std::string_view(frame).substr(2));
  frame.push_back(static_cast<char>(crc >> 8));
  frame.push_back(static_cast<char>(crc & 0xff));
  frame.push_back(static_cast<char>(FOOTER >> 8));
  frame.push_back(static_cast<char>(FOOTER & 0xff));
  return frame;
}

std::string makera_payload(std::string_view frame, std::uint8_t expected_command) {
  require(frame.size() >= 9, "Makera response should contain a complete frame");
  require(static_cast<unsigned char>(frame[0]) == (HEADER >> 8) &&
              static_cast<unsigned char>(frame[1]) == (HEADER & 0xff),
          "Makera response should start with the protocol header");
  const auto data_length = static_cast<std::size_t>(
      (static_cast<unsigned char>(frame[2]) << 8) | static_cast<unsigned char>(frame[3]));
  require(data_length >= 3 && frame.size() >= data_length + 6,
          "Makera response should contain its declared payload");
  require(static_cast<unsigned char>(frame[4]) == expected_command,
          "Makera response should use the expected packet type");
  require(static_cast<unsigned char>(frame[data_length + 4]) == (FOOTER >> 8) &&
              static_cast<unsigned char>(frame[data_length + 5]) == (FOOTER & 0xff),
          "Makera response should end with the protocol footer");
  const auto received_crc = static_cast<std::uint16_t>(
      (static_cast<unsigned char>(frame[data_length + 2]) << 8) |
      static_cast<unsigned char>(frame[data_length + 3]));
  require(crc16_ccitt(frame.substr(2, data_length)) == received_crc,
          "Makera response should have a valid CRC");
  return std::string(frame.substr(5, data_length - 3));
}

}  // namespace

int main() {
  sim::SimulationInstance simulation;
  auto& runtime = simulation.firmware();
  runtime.boot();
  require(communication_protocol == PROTOCOL_MAKERA,
          "stock C1 firmware configuration should select Makera framing");

  auto& wifi = simulation.machine().context().m8266_wifi();
  require(wifi.tcp_server_port() == 2222, "real WifiProvider should configure the simulated M8266 TCP server");
  require(wifi.udp_listen_port() == 4444, "real WifiProvider should configure the simulated M8266 UDP listener");

  wifi.connect_tcp_client();
  runtime.io().write_wifi_tcp(makera_frame(PTYPE_CTRL_SINGLE, "?"));
  std::string response;
  for (int i = 0; i < 20 && response.empty(); ++i) {
    runtime.runner().run_main_loop(1);
    response += runtime.io().read_wifi_tcp();
  }

  require(!wifi.has_received_data(), "WifiProvider should consume the Makera request frame");
  const auto status = makera_payload(response, PTYPE_STATUS_RES);
  require(status.find("<") != std::string::npos, "WiFi TCP should return firmware status through M8266 send APIs");
  require(status.find("MPos:") != std::string::npos, "WiFi TCP status should come from the real firmware query path");

  return 0;
}
