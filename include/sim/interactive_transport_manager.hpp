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

#ifndef SIMULATOR_SIM_INTERACTIVE_TRANSPORT_MANAGER_HPP
#define SIMULATOR_SIM_INTERACTIVE_TRANSPORT_MANAGER_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "carvera_sim.pb.h"
#include "sim/interactive_io.hpp"

namespace sim {

class MachineSimulator;
class RuntimeIo;
class RuntimePump;

struct InteractiveTransportStartResult {
  bool ok{true};
  std::string error;
  carvera::sim::v1::InteractiveTransport transport;
};

class InteractiveTransportManager {
 public:
  using AuxiliaryPump = std::function<void()>;
  using UploadingQuery = std::function<bool()>;

  InteractiveTransportManager(RuntimeIo& io, RuntimePump& runner, MachineSimulator& machine, UploadingQuery uploading);

  InteractiveTransportStartResult start(const carvera::sim::v1::StartInteractiveTransport& command);
  void stop();
  bool active() const;
  void pump();
  void fill(carvera::sim::v1::InteractiveTransport& transport) const;
  void set_auxiliary_pump(AuxiliaryPump pump);

 private:
  void relay_wifi_discovery();
  void log_traffic(const char* channel, const char* direction, const std::string& bytes) const;

  RuntimeIo& runtime_io_;
  RuntimePump& runner_;
  MachineSimulator& machine_;
  UploadingQuery uploading_;
  std::unique_ptr<VirtualComPort> uart_;
  std::vector<std::unique_ptr<LocalhostTcpBridge>> tcp_bridges_;
  LocalhostDiscoveryBeacon discovery_beacon_;
  AuxiliaryPump auxiliary_pump_;
  bool traffic_logging_{false};
};

}  // namespace sim

#endif
