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

#include <sstream>

#include "sim/api_conversions.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/logging.hpp"

namespace sim {
namespace {

const char* machine_model_name(MachineModel model) {
  switch (model) {
    case MachineModel::CarveraC1:
      return "C1";
    case MachineModel::CarveraAirCA1:
      return "CA1";
  }
  return "unknown";
}

}  // namespace

std::optional<ApiService::Response> ApiService::handle_lifecycle_command(const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kReset:
      firmware_.reset();
      logging::event("lifecycle", "firmware reset requested");
      return ok(request.id());
    case Request::kPoll:
      simulator_.poll();
      return ok(request.id());
    case Request::kGetStatus: {
      auto response = ok(request.id());
      auto* status = response.mutable_status();
      const auto factory_settings = firmware_.factory_settings();
      status->set_time_us(simulator_.time_us());
      status->set_time_mode(api::time_mode(simulator_.is_realtime()));
      status->set_machine_model(api::proto_machine_model(factory_settings.machine_model));
      status->set_function_setting(factory_settings.function_setting);
      status->set_realtime_speed(simulator_.realtime_speed());
      return response;
    }
    case Request::kSetTimeMode:
      switch (request.set_time_mode().mode()) {
        case carvera::sim::v1::TIME_MODE_MANUAL:
          simulator_.pause_realtime();
          logging::event("lifecycle", "time mode manual");
          return ok(request.id());
        case carvera::sim::v1::TIME_MODE_REALTIME:
          simulator_.start_realtime();
          logging::event("lifecycle", "time mode realtime");
          return ok(request.id());
        default:
          return error(request.id(), "unsupported time mode");
      }
    case Request::kSetRealtimeSpeed: {
      const double multiplier = request.set_realtime_speed().multiplier();
      if (!simulator_.set_realtime_speed(multiplier)) {
        return error(request.id(), "realtime speed multiplier must be > 0 and <= 100");
      }
      std::ostringstream message;
      message << "realtime speed=" << multiplier << "x";
      logging::event("lifecycle", message.str());
      return ok(request.id());
    }
    case Request::kAdvanceTime:
      simulator_.advance_us(request.advance_time().delta_us());
      return ok(request.id());
    case Request::kMountFilesystem: {
      const auto& mount = request.mount_filesystem();
      if (mount.name().empty()) {
        return error(request.id(), "mount name is required");
      }
      if (mount.host_path().empty()) {
        return error(request.id(), "mount host_path is required");
      }
      host_filesystem::mount(mount.name(), mount.host_path());
      logging::event("filesystem", "mounted /" + mount.name() + " -> " + mount.host_path());
      return ok(request.id());
    }
    case Request::kSetMachineModel: {
      const auto model = api::machine_model(request.set_machine_model().machine_model());
      if (model == MachineModel{}) {
        return error(request.id(), "unsupported machine model");
      }
      if (request.set_machine_model().function_setting() > 0xff) {
        return error(request.id(), "function_setting must fit in one byte");
      }
      if (!firmware_.set_factory_settings(FactorySettings{
              model,
              static_cast<std::uint8_t>(request.set_machine_model().function_setting()),
          })) {
        return error(request.id(), "machine model cannot change after firmware boot");
      }
      std::ostringstream message;
      message << "selected model=" << machine_model_name(model)
              << " function_setting=" << request.set_machine_model().function_setting();
      logging::event("lifecycle", message.str());
      return ok(request.id());
    }
    default:
      return std::nullopt;
  }
}

std::optional<ApiService::Response> ApiService::handle_cooperative_lifecycle_command(
    const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kSetRealtimeSpeed:
      return handle_lifecycle_command(request);
    default:
      return std::nullopt;
  }
}

}  // namespace sim
