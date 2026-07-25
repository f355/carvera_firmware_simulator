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

#include "sim/runtime_io.hpp"

#include <utility>

#include "libs/Kernel.h"
#include "modules/communication/SerialConsole.h"
#include "modules/communication/SerialConsole2.h"
#include "StreamOutput.h"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/simulator_context.hpp"

namespace sim {

RuntimeIo::RuntimeIo(MachineSimulator& simulator, RuntimeBootSession& boot_session, BootCallback boot)
    : simulator_(simulator), boot_session_(boot_session), boot_(std::move(boot)) {}

void RuntimeIo::write_serial(const std::string& data) {
  auto& kernel = boot_();
  if (kernel.serial != nullptr && kernel.serial->serial != nullptr) {
    kernel.serial->serial->simulate_rx(data);
  }
}

std::string RuntimeIo::read_serial() {
  auto& kernel = boot_();
  if (kernel.serial == nullptr || kernel.serial->serial == nullptr) {
    return "";
  }
  return kernel.serial->serial->take_tx();
}

void RuntimeIo::write_serial_command(const std::string& command) {
  boot_();
  write_serial(communication_protocol == PROTOCOL_MAKERA ? makera::encode_console_input(command) : command);
}

std::string RuntimeIo::read_serial_text() {
  auto bytes = read_serial();
  if (communication_protocol == PROTOCOL_SMOOTHIE) {
    return bytes;
  }
  serial_decoder_.append(bytes);
  (void)serial_decoder_.take_frames();
  return serial_decoder_.take_text();
}

void RuntimeIo::write_wifi_tcp(const std::string& data) {
  auto& wifi = simulator_.context().m8266_wifi();
  const bool had_client = wifi.has_tcp_client();
  boot_();
  if (had_client) {
    wifi.connect_tcp_client();
  }
  wifi.receive_tcp(data);
}

std::string RuntimeIo::read_wifi_tcp() {
  boot_();
  return simulator_.context().m8266_wifi().take_tcp_tx();
}

void RuntimeIo::write_wifi_command(const std::string& command) {
  boot_();
  write_wifi_tcp(communication_protocol == PROTOCOL_MAKERA ? makera::encode_console_input(command) : command);
}

std::string RuntimeIo::read_wifi_text() {
  auto bytes = read_wifi_tcp();
  if (communication_protocol == PROTOCOL_SMOOTHIE) {
    return bytes;
  }
  wifi_decoder_.append(bytes);
  (void)wifi_decoder_.take_frames();
  return wifi_decoder_.take_text();
}

void RuntimeIo::set_wifi_client_connected(bool connected) {
  auto& wifi = simulator_.context().m8266_wifi();
  if (connected) {
    wifi.connect_tcp_client();
  } else {
    wifi.disconnect_tcp_client();
  }
}

std::vector<std::string> RuntimeIo::take_wifi_udp_datagrams() {
  return simulator_.context().m8266_wifi().take_udp_tx_datagrams();
}

void RuntimeIo::write_wireless_probe_rx(const std::string& data) {
  auto& kernel = boot_();
  (void)kernel;
  auto* serial = boot_session_.wireless_probe_serial();
  if (serial != nullptr && serial->serial != nullptr) {
    serial->serial->simulate_rx(data);
  }
}

std::string RuntimeIo::read_wireless_probe_tx() {
  auto& kernel = boot_();
  (void)kernel;
  auto* serial = boot_session_.wireless_probe_serial();
  if (serial == nullptr || serial->serial == nullptr) {
    return "";
  }
  return serial->serial->take_tx();
}

}  // namespace sim
