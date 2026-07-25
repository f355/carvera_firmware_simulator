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

#include <sstream>

#include "sim/framed_proto.hpp"
#include "support/api_service_harness.hpp"
#include "support/posix_io.hpp"
#include "support/assertions.hpp"

using sim::test::require;


int main() {
  sim::test::ApiHarness api;

  auto response = api.request([](auto& request) { request.mutable_advance_time()->set_delta_us(123); });
  require(response.ok(), "advance_time should succeed");
  require(response.id() == 1, "response should preserve request id");

  response = api.request([&](auto& request) {
    request.mutable_get_status();
  });
  require(response.ok(), "get_status should succeed");
  require(response.status().time_us() == 123, "status should report simulator time");
  require(response.status().time_mode() == carvera::sim::v1::TIME_MODE_MANUAL, "default time mode should be manual");
  require(response.status().realtime_speed() == 1.0, "default realtime speed should be 1x");
  require(response.status().machine_model() == carvera::sim::v1::MACHINE_MODEL_CARVERA_C1,
          "default machine model should be C1");

  response = api.request([&](auto& request) {
    request.mutable_set_realtime_speed()->set_multiplier(5.0);
  });
  require(response.ok(), "set_realtime_speed should accept positive multipliers");

  response = api.request([&](auto& request) {
    request.mutable_get_status();
  });
  require(response.ok(), "get_status after set_realtime_speed should succeed");
  require(response.status().realtime_speed() == 5.0, "status should report configured realtime speed");

  response = api.request([&](auto& request) {
    request.mutable_set_time_mode()->set_mode(carvera::sim::v1::TIME_MODE_REALTIME);
  });
  require(response.ok(), "set_time_mode realtime should succeed");

  response = api.request([&](auto& request) {
    request.mutable_reset();
  });
  require(response.ok(), "firmware reset should succeed");

  response = api.request([&](auto& request) {
    request.mutable_get_status();
  });
  require(response.ok(), "get_status after firmware reset should succeed");
  require(response.status().time_mode() == carvera::sim::v1::TIME_MODE_REALTIME,
          "firmware reset should preserve host realtime mode");
  require(response.status().realtime_speed() == 5.0, "firmware reset should preserve host realtime speed");

  response = api.request([&](auto& request) {
    request.mutable_set_realtime_speed()->set_multiplier(0.0);
  });
  require(!response.ok(), "set_realtime_speed should reject non-positive multipliers");

  response = api.request([&](auto& request) {
    request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1);
    request.mutable_set_machine_model()->set_function_setting(0);
  });
  require(response.ok(), "set_machine_model should succeed before firmware boots");

  response = api.request([&](auto& request) {
    request.mutable_get_status();
  });
  require(response.ok(), "get_status after set_machine_model should succeed");
  require(response.status().machine_model() == carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1,
          "status should report configured CA1 model");
  require(response.status().function_setting() == 0, "status should report configured function flags");

  response = api.request([&](auto& request) {
    request.mutable_start_interactive_transport()->set_enable_uart(false);
    request.mutable_start_interactive_transport()->add_tcp_ports(0);
  });
  require(response.ok(), "start_interactive_transport should start a localhost TCP bridge");
  require(response.interactive_transport().tcp_endpoints_size() == 1,
          "start_interactive_transport should report the TCP endpoint");
  const auto first_tcp_port = response.interactive_transport().tcp_endpoints(0).port();
  require(first_tcp_port > 0, "start_interactive_transport should report a bound TCP port");
  require(sim::test::localhost_accepts_tcp(first_tcp_port),
          "started interactive TCP bridge should accept localhost connects");

  response = api.request([&](auto& request) {
    request.mutable_stop_interactive_transport();
  });
  require(response.ok(), "stop_interactive_transport should stop active endpoints");
  require(!sim::test::localhost_accepts_tcp(first_tcp_port),
          "stopped interactive TCP bridge should close its localhost port");

  response = api.request([&](auto& request) {
    request.mutable_start_interactive_transport()->set_enable_uart(false);
    request.mutable_start_interactive_transport()->add_tcp_ports(first_tcp_port);
  });
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
