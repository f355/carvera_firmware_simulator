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

#ifndef SIMULATOR_SIM_API_SERVICE_HPP
#define SIMULATOR_SIM_API_SERVICE_HPP

#include <functional>
#include <optional>
#include <string>

#include "carvera_sim.pb.h"
#include "sim/interactive_transport_manager.hpp"

namespace sim {

class FirmwareRuntime;
class MachineSimulator;
class PersistentMachineState;
class PhysicalScene;
class RuntimeIo;
class RuntimePhysicalControls;
class RuntimePump;
class SimulationInstance;

class ApiService {
 public:
  explicit ApiService(SimulationInstance& simulation);

  carvera::sim::v1::Response handle(const carvera::sim::v1::Request& request);
  carvera::sim::v1::Response handle_cooperative(const carvera::sim::v1::Request& request);
  bool fill_machine_snapshot_nonblocking(carvera::sim::v1::MachineSnapshot& snapshot);
  bool fill_physical_io_snapshot_nonblocking(carvera::sim::v1::PhysicalIoSnapshot& snapshot);
  void set_auxiliary_interactive_pump(std::function<void()> pump);
  void pump_interactive();

 private:
  using Response = carvera::sim::v1::Response;

  std::optional<Response> handle_lifecycle_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_cooperative_lifecycle_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_harness_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_cooperative_harness_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_serial_motion_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_physical_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_cooperative_physical_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_snapshot_transport_command(const carvera::sim::v1::Request& request);
  std::optional<Response> handle_cooperative_snapshot_transport_command(const carvera::sim::v1::Request& request);

  carvera::sim::v1::Response ok(std::uint64_t request_id) const;
  carvera::sim::v1::Response error(std::uint64_t request_id, const char* message) const;
  void fill_physical_io_snapshot(carvera::sim::v1::PhysicalIoSnapshot& snapshot);

  FirmwareRuntime& firmware_;
  RuntimePhysicalControls& inputs_;
  PhysicalScene& world_;
  RuntimeIo& io_;
  RuntimePump& runner_;
  MachineSimulator& machine_;
  PersistentMachineState& persistent_state_;
  InteractiveTransportManager interactive_transport_;
};

}  // namespace sim

#endif
