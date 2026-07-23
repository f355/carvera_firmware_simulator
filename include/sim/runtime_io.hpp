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

#ifndef SIMULATOR_SIM_RUNTIME_IO_HPP
#define SIMULATOR_SIM_RUNTIME_IO_HPP

#include <functional>
#include <string>
#include <vector>

#include "sim/makera_protocol.hpp"

class Kernel;

namespace sim {

class RuntimeBootSession;
class MachineSimulator;

class RuntimeIo {
 public:
  using BootCallback = std::function<Kernel&()>;

  RuntimeIo(MachineSimulator& simulator, RuntimeBootSession& boot_session, BootCallback boot);

  void write_serial(const std::string& data);
  std::string read_serial();
  void write_serial_command(const std::string& command);
  std::string read_serial_text();
  void write_wifi_tcp(const std::string& data);
  std::string read_wifi_tcp();
  void write_wifi_command(const std::string& command);
  std::string read_wifi_text();
  void set_wifi_client_connected(bool connected);
  std::vector<std::string> take_wifi_udp_datagrams();
  void write_wireless_probe_rx(const std::string& data);
  std::string read_wireless_probe_tx();

 private:
  MachineSimulator& simulator_;
  RuntimeBootSession& boot_session_;
  BootCallback boot_;
  makera::FrameDecoder serial_decoder_;
  makera::FrameDecoder wifi_decoder_;
};

}  // namespace sim

#endif
