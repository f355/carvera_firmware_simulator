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

#include "sim/api_conversions.hpp"

namespace sim {

std::optional<ApiService::Response> ApiService::handle_serial_motion_command(const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kWriteSerial:
      firmware_.write_serial(request.write_serial().data());
      return ok(request.id());
    case Request::kReadSerial: {
      auto response = ok(request.id());
      response.mutable_serial_data()->set_data(firmware_.read_serial());
      return response;
    }
    case Request::kRunUntilIdle: {
      const auto max_step_ticks =
          request.run_until_idle().max_step_ticks() == 0 ? 100000 : request.run_until_idle().max_step_ticks();
      const bool idle = firmware_.run_until_idle(static_cast<std::size_t>(max_step_ticks));
      auto response = ok(request.id());
      response.mutable_run_result()->set_idle(idle);
      if (!idle) {
        response.set_ok(false);
        response.set_error("motion did not reach idle");
      }
      return response;
    }
    case Request::kJog: {
      const auto command = api::jog_command(request.jog());
      if (command.empty()) {
        return error(request.id(), "invalid jog command");
      }

      firmware_.write_serial("G91\n");
      firmware_.write_serial(command);
      const auto max_step_ticks = request.jog().max_step_ticks() == 0 ? 100000 : request.jog().max_step_ticks();
      const bool idle = firmware_.run_until_idle(static_cast<std::size_t>(max_step_ticks));
      auto response = ok(request.id());
      auto* result = response.mutable_jog_result();
      result->set_idle(idle);
      result->set_serial_data(firmware_.read_serial());
      if (!idle) {
        response.set_ok(false);
        response.set_error("jog did not reach idle");
      }
      return response;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace sim
