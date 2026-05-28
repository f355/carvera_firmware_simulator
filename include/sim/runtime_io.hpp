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

class Kernel;

namespace sim {

class RuntimeBootSession;

class RuntimeIo {
 public:
  using BootCallback = std::function<Kernel&()>;

  RuntimeIo(RuntimeBootSession& boot_session, BootCallback boot);

  void write_serial(const std::string& data);
  std::string read_serial();
  void write_wifi_tcp(const std::string& data);
  std::string read_wifi_tcp();
  void write_wireless_probe_rx(const std::string& data);
  std::string read_wireless_probe_tx();

 private:
  RuntimeBootSession& boot_session_;
  BootCallback boot_;
};

}  // namespace sim

#endif
