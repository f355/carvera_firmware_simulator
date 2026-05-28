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

#include "sim/api_snapshot.hpp"

namespace sim {

std::optional<ApiService::Response> ApiService::handle_snapshot_transport_command(
    const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kGetMachineSnapshot: {
      auto response = ok(request.id());
      if (!fill_machine_snapshot(*response.mutable_machine_snapshot(), firmware_, simulator_)) {
        return error(request.id(), "firmware Robot is not available");
      }
      return response;
    }
    case Request::kStartInteractiveTransport: {
      const auto result = interactive_transport_.start(request.start_interactive_transport());
      if (!result.ok) {
        return error(request.id(), result.error.c_str());
      }
      auto response = ok(request.id());
      response.mutable_interactive_transport()->CopyFrom(result.transport);
      return response;
    }
    case Request::kStopInteractiveTransport:
      interactive_transport_.stop();
      return ok(request.id());
    default:
      return std::nullopt;
  }
}

}  // namespace sim
