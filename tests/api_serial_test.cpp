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

#include <string>

#include "sim/makera_protocol.hpp"
#include "support/api_service_harness.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

std::string decode_text(sim::makera::FrameDecoder& decoder, const std::string& bytes) {
  decoder.append(bytes);
  auto text = decoder.take_text();
  (void)decoder.take_frames();
  return text;
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_api_serial_test");
  sim::test::CartesianConfigOptions config;
  config.include_rotary_axes = true;
  config.extra =
      "alpha_min_endstop 1.1^\n"
      "alpha_limit_enable true\n"
      "beta_min_endstop 1.2^\n"
      "beta_limit_enable true\n"
      "gamma_min_endstop 1.3^\n"
      "gamma_limit_enable true\n";
  sd.write_config(sim::test::cartesian_config(config));

  sim::test::ApiHarness api;

  carvera::sim::v1::Response response;
  sim::makera::FrameDecoder serial_decoder;

  response = api.request([&](auto& request) {
    request.mutable_mount_filesystem()->set_name("sd");
    request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  });
  require(response.ok(), "mount_filesystem should succeed");

  response = api.request([&](auto& request) {
    request.mutable_write_serial()->set_data(sim::makera::encode_console_input("G91\nG0 X-1 F1500\n"));
  });
  require(response.ok(), "write_serial should succeed");

  response = api.request([&](auto& request) {
    request.mutable_run_until_idle()->set_max_step_ticks(100'000);
  });
  require(response.ok(), "run_until_idle should succeed");
  require(response.run_result().idle(), "firmware should reach motion idle");

  response = api.request([&](auto& request) {
    request.mutable_get_axis_position()->set_axis(0);
  });
  require(response.ok(), "get_axis_position should succeed");
  require(response.axis_position().steps() < 0,
          "API-fed serial jog should move the physical X axis into negative travel");

  response = api.request([&](auto& request) {
    request.mutable_read_serial();
  });
  require(response.ok(), "read_serial should succeed");
  require(decode_text(serial_decoder, response.serial_data().data()).find("ok") != std::string::npos,
          "serial output should include G-code acknowledgements");

  response = api.request([&](auto& request) {
    request.mutable_write_serial()->set_data(sim::makera::encode_console_input("$J X-10 F10000\n"));
  });
  require(response.ok(), "controller-style X jog should be accepted");

  response = api.request([&](auto& request) {
    request.mutable_run_until_idle()->set_max_step_ticks(200'000);
  });
  require(response.ok(), "controller-style X jog should not hit the opposite hard limit");
  require(response.run_result().idle(), "controller-style X jog should reach idle");

  response = api.request([&](auto& request) {
    request.mutable_read_serial();
  });
  require(response.ok(), "read_serial after controller-style jog should succeed");
  require(decode_text(serial_decoder, response.serial_data().data()).find("Limit switch") == std::string::npos,
          "negative X jog inside travel should not trip a hard limit");

  response = api.request([&](auto& request) {
    request.mutable_write_serial()->set_data(sim::makera::encode_console_input("$J X5 F10000\n"));
  });
  require(response.ok(), "controller-style positive X jog should be accepted");

  response = api.request([&](auto& request) {
    request.mutable_run_until_idle()->set_max_step_ticks(200'000);
  });
  require(response.ok(), "controller-style positive X jog inside travel should not hit a hard limit");
  require(response.run_result().idle(), "controller-style positive X jog should reach idle");

  response = api.request([&](auto& request) {
    request.mutable_read_serial();
  });
  require(response.ok(), "read_serial after positive controller-style jog should succeed");
  require(decode_text(serial_decoder, response.serial_data().data()).find("Limit switch") == std::string::npos,
          "positive X jog inside travel should not trip a hard limit");

  response = api.request([&](auto& request) {
    request.mutable_write_serial()->set_data(sim::makera::encode_console_input("reset\n"));
  });
  require(response.ok(), "reset shell command should be accepted");

  for (int i = 0; i < 20; ++i) {
    response = api.request([](auto& request) { request.mutable_run_until_idle()->set_max_step_ticks(100'000); });
    require(response.ok(), "run_until_idle should pump reset delay");
  }

  response = api.request([&](auto& request) {
    request.mutable_write_serial()->set_data(sim::makera::encode_console_input("?"));
  });
  require(response.ok(), "status query after reset should be accepted");

  response = api.request([&](auto& request) {
    request.mutable_run_until_idle()->set_max_step_ticks(100'000);
  });
  require(response.ok(), "firmware should run after reset");

  response = api.request([&](auto& request) {
    request.mutable_read_serial();
  });
  require(response.ok(), "read_serial after reset should succeed");
  require(decode_text(serial_decoder, response.serial_data().data()).find("<Idle") != std::string::npos,
          "reset should reboot firmware back to an idle controller-visible state");
  return 0;
}
