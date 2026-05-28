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

#include <iostream>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/framed_proto.hpp"
#include "sim/machine_simulator.hpp"

int main() {
  sim::MachineSimulator simulator;
  sim::ApiService api(simulator);

  carvera::sim::v1::Request request;
  while (sim::proto_framing::read_message(std::cin, request)) {
    const auto response = api.handle(request);
    if (!sim::proto_framing::write_message(std::cout, response)) {
      return 1;
    }
    std::cout.flush();
    request.Clear();
  }

  return std::cin.eof() ? 0 : 1;
}
