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
#include <fstream>
#include <iostream>

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void write_switch_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "sd_ok true\n"
         << "switch.light.enable true\n"
         << "switch.light.output_pin 2.0\n"
         << "switch.light.output_type digital\n"
         << "switch.light.startup_state false\n"
         << "switch.powerfan.enable true\n"
         << "switch.powerfan.output_pin 2.3\n"
         << "switch.powerfan.output_type hwpwm\n"
         << "switch.powerfan.default_on_value 30\n";
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "carvera_sim_switch_api_test";
  std::filesystem::remove_all(root);
  write_switch_config(root);
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);

  sim::MachineSimulator simulator;
  sim::ApiService api(simulator);
  carvera::sim::v1::Request request;

  request.set_id(1);
  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1);
  request.mutable_set_machine_model()->set_function_setting(0);
  auto response = api.handle(request);
  require(response.ok(), "CA1 model should configure before boot");

  request.Clear();
  request.set_id(2);
  request.mutable_set_switch_state()->set_name(carvera::sim::v1::SWITCH_NAME_LIGHT);
  request.mutable_set_switch_state()->set_on(true);
  response = api.handle(request);
  require(response.ok(), "set_switch_state should control enabled firmware switches on CA1 without ATC");

  request.Clear();
  request.set_id(3);
  request.mutable_get_switch_state()->set_name(carvera::sim::v1::SWITCH_NAME_LIGHT);
  response = api.handle(request);
  require(response.ok(), "get_switch_state should read enabled firmware switches");
  require(response.switch_state().available(), "light switch should be available from firmware PublicData");
  require(response.switch_state().on(), "light switch should report the state set through PublicData");

  request.Clear();
  request.set_id(4);
  request.mutable_get_gpio_level()->mutable_pin()->set_port(2);
  request.mutable_get_gpio_level()->mutable_pin()->set_pin(0);
  response = api.handle(request);
  require(response.ok(), "GPIO readback should succeed for the configured light pin");
  require(response.gpio_level().high(), "light switch should drive the configured output pin high");

  request.Clear();
  request.set_id(5);
  auto* powerfan = request.mutable_set_switch_state();
  powerfan->set_name(carvera::sim::v1::SWITCH_NAME_POWER_FAN);
  powerfan->set_on(true);
  powerfan->set_has_value(true);
  powerfan->set_value(42.0);
  response = api.handle(request);
  require(response.ok(), "set_switch_state should pass PWM-style values through firmware switches");

  request.Clear();
  request.set_id(6);
  request.mutable_get_switch_state()->set_name(carvera::sim::v1::SWITCH_NAME_POWER_FAN);
  response = api.handle(request);
  require(response.ok(), "get_switch_state should read PWM-style firmware switches");
  require(response.switch_state().available(), "power fan switch should be available");
  require(response.switch_state().on(), "power fan should report on");
  require(response.switch_state().value() == 42.0, "power fan should preserve the requested switch value");

  request.Clear();
  request.set_id(7);
  request.mutable_set_switch_state()->set_name(carvera::sim::v1::SWITCH_NAME_LIGHT);
  request.mutable_set_switch_state()->set_on(false);
  response = api.handle(request);
  require(response.ok(), "set_switch_state should turn firmware switches off");

  request.Clear();
  request.set_id(8);
  request.mutable_get_switch_state()->set_name(carvera::sim::v1::SWITCH_NAME_LIGHT);
  response = api.handle(request);
  require(response.ok(), "get_switch_state after off should succeed");
  require(!response.switch_state().on(), "light switch should report off after being cleared");

  std::filesystem::remove_all(root);
  return 0;
}
