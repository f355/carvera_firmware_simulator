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
#include "sim/m8266_wifi.hpp"
#include "sim/runtime_boot_session.hpp"

namespace sim {

RuntimeIo::RuntimeIo(RuntimeBootSession& boot_session, BootCallback boot)
    : boot_session_(boot_session), boot_(std::move(boot)) {}

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

void RuntimeIo::write_wifi_tcp(const std::string& data) {
  const bool had_client = m8266_wifi::active().has_tcp_client();
  boot_();
  if (had_client) {
    m8266_wifi::active().connect_tcp_client();
  }
  m8266_wifi::active().receive_tcp(data);
}

std::string RuntimeIo::read_wifi_tcp() {
  boot_();
  return m8266_wifi::active().take_tcp_tx();
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
