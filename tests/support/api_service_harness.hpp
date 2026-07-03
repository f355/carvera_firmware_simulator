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

#ifndef SIMULATOR_TESTS_SUPPORT_API_SERVICE_HARNESS_HPP
#define SIMULATOR_TESTS_SUPPORT_API_SERVICE_HARNESS_HPP

#include <cstdint>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/simulation_instance.hpp"

namespace sim::test {

namespace pb = carvera::sim::v1;

class ApiHarness {
 public:
  ApiHarness() = default;
  explicit ApiHarness(const PersistentMachineConfig& persistent_config)
      : simulation_(persistent_config), api_(simulation_) {}

  template <typename FillRequest>
  pb::Response request(FillRequest fill_request) {
    pb::Request request;
    request.set_id(next_id_++);
    fill_request(request);
    return api_.handle(request);
  }

  MachineSimulator& simulator() { return simulation_.machine(); }
  ApiService& api() { return api_; }

 private:
  SimulationInstance simulation_;
  ApiService api_{simulation_};
  std::uint32_t next_id_{1};
};

}  // namespace sim::test

#endif
