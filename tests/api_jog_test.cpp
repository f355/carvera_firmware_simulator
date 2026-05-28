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
#include <filesystem>
#include <iostream>
#include <string>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/machine_simulator.hpp"
#include "support/cartesian_config.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "carvera_sim_api_jog_test";
  std::filesystem::remove_all(root);
  sim::test::CartesianConfigOptions config;
  config.include_rotary_axes = true;
  sim::test::write_cartesian_config(root, config);

  sim::MachineSimulator simulator;
  sim::ApiService api(simulator);

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.set_id(1);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(root.string());
  response = api.handle(request);
  require(response.ok(), "mount_filesystem should succeed");

  request.Clear();
  request.set_id(3);
  auto* jog = request.mutable_jog();
  auto* delta = jog->add_delta();
  delta->set_axis(carvera::sim::v1::AXIS_X);
  delta->set_distance(-1.0);
  jog->set_feed_rate(1500.0);
  jog->set_max_step_ticks(100'000);
  response = api.handle(request);
  require(response.ok(), "jog should succeed");
  require(response.jog_result().idle(), "jog should run firmware motion to idle");
  require(response.jog_result().serial_data().find("ok") != std::string::npos,
          "jog should return serial acknowledgements");

  request.Clear();
  request.set_id(4);
  request.mutable_get_axis_position()->set_axis(0);
  response = api.handle(request);
  require(response.ok(), "get_axis_position should succeed");
  require(std::abs(response.axis_position().steps()) >= 100, "typed jog should move the physical X axis");

  std::filesystem::remove_all(root);
  return 0;
}
