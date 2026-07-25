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

#include <filesystem>
#include <string>

#include "support/api_service_harness.hpp"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

using sim::test::require;

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_api_jog_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.include_rotary_axes = true;
  sim::test::write_cartesian_config(root, config);

  sim::test::ApiHarness api;

  auto response = api.request([&root](auto& request) {
    request.mutable_mount_filesystem()->set_name("sd");
    request.mutable_mount_filesystem()->set_host_path(root.string());
  });
  require(response.ok(), "mount_filesystem should succeed");

  response = api.request([](auto& request) { request.mutable_get_machine_snapshot(); });
  require(response.ok() && response.machine_snapshot().homed(), "firmware should boot and home before the jog");

  response = api.request([](auto& request) { request.mutable_get_axis_position()->set_axis(0); });
  require(response.ok(), "initial get_axis_position should succeed");
  const auto initial_x_steps = response.axis_position().steps();

  response = api.request([](auto& request) {
    auto* jog = request.mutable_jog();
    auto* delta = jog->add_delta();
    delta->set_axis(carvera::sim::v1::AXIS_X);
    delta->set_distance(-1.0);
    jog->set_feed_rate(1500.0);
    jog->set_max_step_ticks(100'000);
  });
  require(response.ok(), "jog should succeed");
  require(response.jog_result().idle(), "jog should run firmware motion to idle");
  require(response.jog_result().serial_data().find("ok") != std::string::npos,
          "jog should return serial acknowledgements");

  response = api.request([](auto& request) { request.mutable_get_axis_position()->set_axis(0); });
  require(response.ok(), "get_axis_position should succeed");
  require(std::abs(response.axis_position().steps() - initial_x_steps) >= 100,
          "typed jog should move the physical X axis from its starting position");
  return 0;
}
