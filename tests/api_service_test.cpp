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

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/framed_proto.hpp"
#include "sim/machine_simulator.hpp"
#include "support/posix_io.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::ApiService api(simulator);

  carvera::sim::v1::Request request;
  request.set_id(10);
  request.mutable_advance_time()->set_delta_us(123);
  auto response = api.handle(request);
  require(response.ok(), "advance_time should succeed");
  require(response.id() == 10, "response should preserve request id");

  request.Clear();
  request.set_id(11);
  request.mutable_get_status();
  response = api.handle(request);
  require(response.ok(), "get_status should succeed");
  require(response.status().time_us() == 123, "status should report simulator time");
  require(response.status().time_mode() == carvera::sim::v1::TIME_MODE_MANUAL, "default time mode should be manual");
  require(response.status().machine_model() == carvera::sim::v1::MACHINE_MODEL_CARVERA_C1,
          "default machine model should be C1");

  request.Clear();
  request.set_id(120);
  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1);
  request.mutable_set_machine_model()->set_function_setting(0);
  response = api.handle(request);
  require(response.ok(), "set_machine_model should succeed before firmware boots");

  request.Clear();
  request.set_id(121);
  request.mutable_get_status();
  response = api.handle(request);
  require(response.ok(), "get_status after set_machine_model should succeed");
  require(response.status().machine_model() == carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1,
          "status should report configured CA1 model");
  require(response.status().function_setting() == 0, "status should report configured function flags");

  request.Clear();
  request.set_id(122);
  request.mutable_start_interactive_transport()->set_enable_uart(false);
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  response = api.handle(request);
  require(response.ok(), "start_interactive_transport should start a localhost TCP bridge");
  require(response.interactive_transport().tcp_endpoints_size() == 1,
          "start_interactive_transport should report the TCP endpoint");
  const auto first_tcp_port = response.interactive_transport().tcp_endpoints(0).port();
  require(first_tcp_port > 0, "start_interactive_transport should report a bound TCP port");
  require(sim::test::localhost_accepts_tcp(first_tcp_port),
          "started interactive TCP bridge should accept localhost connects");

  request.Clear();
  request.set_id(123);
  request.mutable_stop_interactive_transport();
  response = api.handle(request);
  require(response.ok(), "stop_interactive_transport should stop active endpoints");
  require(!sim::test::localhost_accepts_tcp(first_tcp_port),
          "stopped interactive TCP bridge should close its localhost port");

  request.Clear();
  request.set_id(124);
  request.mutable_start_interactive_transport()->set_enable_uart(false);
  request.mutable_start_interactive_transport()->add_tcp_ports(first_tcp_port);
  response = api.handle(request);
  require(response.ok(), "start_interactive_transport should restart after an explicit stop");
  require(response.interactive_transport().tcp_endpoints_size() == 1,
          "restarted interactive transport should report a fresh TCP endpoint");
  require(response.interactive_transport().tcp_endpoints(0).port() == first_tcp_port,
          "restarted interactive transport should be able to bind the stopped port");

  std::stringstream stream;
  require(sim::proto_framing::write_message(stream, response), "framed write should succeed");
  carvera::sim::v1::Response decoded;
  require(sim::proto_framing::read_message(stream, decoded), "framed read should succeed");
  require(decoded.id() == response.id(), "framed protobuf should round-trip");

  return 0;
}
