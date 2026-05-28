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
#include <string>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/machine_simulator.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
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

  sim::MachineSimulator simulator;
  sim::ApiService api(simulator);

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.set_id(1);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  response = api.handle(request);
  require(response.ok(), "mount_filesystem should succeed");

  request.Clear();
  request.set_id(3);
  request.mutable_write_serial()->set_data("G91\nG0 X-1 F1500\n");
  response = api.handle(request);
  require(response.ok(), "write_serial should succeed");

  request.Clear();
  request.set_id(4);
  request.mutable_run_until_idle()->set_max_step_ticks(100'000);
  response = api.handle(request);
  require(response.ok(), "run_until_idle should succeed");
  require(response.run_result().idle(), "firmware should reach motion idle");

  request.Clear();
  request.set_id(5);
  request.mutable_get_axis_position()->set_axis(0);
  response = api.handle(request);
  require(response.ok(), "get_axis_position should succeed");
  require(response.axis_position().steps() < 0,
          "API-fed serial jog should move the physical X axis into negative travel");

  request.Clear();
  request.set_id(6);
  request.mutable_read_serial();
  response = api.handle(request);
  require(response.ok(), "read_serial should succeed");
  require(response.serial_data().data().find("ok") != std::string::npos,
          "serial output should include G-code acknowledgements");

  request.Clear();
  request.set_id(7);
  request.mutable_write_serial()->set_data("$J X-10 F10000\n");
  response = api.handle(request);
  require(response.ok(), "controller-style X jog should be accepted");

  request.Clear();
  request.set_id(8);
  request.mutable_run_until_idle()->set_max_step_ticks(200'000);
  response = api.handle(request);
  require(response.ok(), "controller-style X jog should not hit the opposite hard limit");
  require(response.run_result().idle(), "controller-style X jog should reach idle");

  request.Clear();
  request.set_id(9);
  request.mutable_read_serial();
  response = api.handle(request);
  require(response.ok(), "read_serial after controller-style jog should succeed");
  require(response.serial_data().data().find("Limit switch") == std::string::npos,
          "negative X jog inside travel should not trip a hard limit");

  request.Clear();
  request.set_id(10);
  request.mutable_write_serial()->set_data("$J X5 F10000\n");
  response = api.handle(request);
  require(response.ok(), "controller-style positive X jog should be accepted");

  request.Clear();
  request.set_id(11);
  request.mutable_run_until_idle()->set_max_step_ticks(200'000);
  response = api.handle(request);
  require(response.ok(), "controller-style positive X jog inside travel should not hit a hard limit");
  require(response.run_result().idle(), "controller-style positive X jog should reach idle");

  request.Clear();
  request.set_id(12);
  request.mutable_read_serial();
  response = api.handle(request);
  require(response.ok(), "read_serial after positive controller-style jog should succeed");
  require(response.serial_data().data().find("Limit switch") == std::string::npos,
          "positive X jog inside travel should not trip a hard limit");

  request.Clear();
  request.set_id(20);
  request.mutable_write_serial()->set_data("reset\n");
  response = api.handle(request);
  require(response.ok(), "reset shell command should be accepted");

  for (int i = 0; i < 20; ++i) {
    request.Clear();
    request.set_id(21 + i);
    request.mutable_run_until_idle()->set_max_step_ticks(100'000);
    response = api.handle(request);
    require(response.ok(), "run_until_idle should pump reset delay");
  }

  request.Clear();
  request.set_id(50);
  request.mutable_write_serial()->set_data("?\n");
  response = api.handle(request);
  require(response.ok(), "status query after reset should be accepted");

  request.Clear();
  request.set_id(51);
  request.mutable_run_until_idle()->set_max_step_ticks(100'000);
  response = api.handle(request);
  require(response.ok(), "firmware should run after reset");

  request.Clear();
  request.set_id(52);
  request.mutable_read_serial();
  response = api.handle(request);
  require(response.ok(), "read_serial after reset should succeed");
  require(response.serial_data().data().find("<Idle") != std::string::npos,
          "reset should reboot firmware back to an idle controller-visible state");
  return 0;
}
