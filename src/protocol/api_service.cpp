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

#include "sim/api_service.hpp"

#include <utility>

#include "sim/api_snapshot.hpp"
#include "sim/firmware_runtime.hpp"
#include "sim/simulation_instance.hpp"

namespace sim {

ApiService::ApiService(SimulationInstance& simulation)
    : firmware_(simulation.firmware()),
      inputs_(simulation.inputs()),
      world_(simulation.world()),
      io_(simulation.io()),
      runner_(simulation.runner()),
      machine_(simulation.machine()),
      persistent_state_(simulation.persistent_state()),
      interactive_transport_(io_, runner_, machine_, [this]() { return firmware_.is_uploading(); }) {
  interactive_transport_.set_boot_loop_abort([this]() { firmware_.power_off(); });
}

carvera::sim::v1::Response ApiService::handle(const carvera::sim::v1::Request& request) {
  if (auto response = handle_lifecycle_command(request)) {
    return *response;
  }
  if (auto response = handle_harness_command(request)) {
    return *response;
  }
  if (auto response = handle_serial_motion_command(request)) {
    return *response;
  }
  if (auto response = handle_physical_command(request)) {
    return *response;
  }
  if (auto response = handle_snapshot_transport_command(request)) {
    return *response;
  }
  if (request.command_case() == carvera::sim::v1::Request::COMMAND_NOT_SET) {
    return error(request.id(), "command is required");
  }
  return error(request.id(), "unknown command");
}

carvera::sim::v1::Response ApiService::handle_cooperative(const carvera::sim::v1::Request& request) {
  if (auto response = handle_cooperative_lifecycle_command(request)) {
    return *response;
  }
  if (auto response = handle_cooperative_harness_command(request)) {
    return *response;
  }
  if (auto response = handle_cooperative_physical_command(request)) {
    return *response;
  }
  if (request.command_case() == carvera::sim::v1::Request::COMMAND_NOT_SET) {
    return error(request.id(), "command is required");
  }
  return error(request.id(), "simulator busy running firmware command");
}

void ApiService::set_auxiliary_interactive_pump(std::function<void()> pump) {
  interactive_transport_.set_auxiliary_pump(std::move(pump));
}

void ApiService::set_max_boot_loop_soft_resets(std::size_t max_resets) {
  interactive_transport_.set_max_consecutive_soft_resets(max_resets);
}

bool ApiService::fill_machine_snapshot_nonblocking(carvera::sim::v1::MachineSnapshot& snapshot) {
  return sim::fill_machine_snapshot_nonblocking(snapshot, firmware_, machine_);
}

bool ApiService::fill_physical_io_snapshot_nonblocking(carvera::sim::v1::PhysicalIoSnapshot& snapshot) {
  if (!firmware_.booted()) {
    return false;
  }
  fill_physical_io_snapshot(snapshot);
  return true;
}

void ApiService::pump_interactive() { interactive_transport_.pump(); }

carvera::sim::v1::Response ApiService::ok(std::uint64_t request_id) const {
  carvera::sim::v1::Response response;
  response.set_id(request_id);
  response.set_ok(true);
  return response;
}

carvera::sim::v1::Response ApiService::error(std::uint64_t request_id, const char* message) const {
  carvera::sim::v1::Response response;
  response.set_id(request_id);
  response.set_ok(false);
  response.set_error(message);
  return response;
}

}  // namespace sim
