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

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

carvera::sim::v1::Response send(sim::ApiService& api, carvera::sim::v1::Request& request) {
  auto response = api.handle(request);
  require(response.ok(), response.error().c_str());
  request.Clear();
  return response;
}

void seed_sd_with_rotary_enabled_eeprom(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);

  sim::i2c_eeprom::clear_persistent_file();
  sim::i2c_eeprom::reset();
  require(sim::i2c_eeprom::use_persistent_file(root / ".eeprom.bin") == false,
          "fresh EEPROM backing file should start empty");
  sim::i2c_eeprom::configure_factory_settings({sim::MachineModel::CarveraAirCA1, 0x01});
  sim::i2c_eeprom::clear_persistent_file();
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_factory_settings_mount_order_test");
  const auto& sd_root = temp_root.path();
  seed_sd_with_rotary_enabled_eeprom(sd_root);
  sim::host_filesystem::clear_mounts();

  sim::SimulationInstance simulation;
  sim::ApiService api(simulation);

  carvera::sim::v1::Request request;
  request.set_id(1);
  request.mutable_set_machine_model()->set_machine_model(carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1);
  request.mutable_set_machine_model()->set_function_setting(0);
  send(api, request);

  request.set_id(2);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd_root.string());
  send(api, request);

  request.set_id(3);
  request.mutable_get_machine_snapshot();
  send(api, request);

  request.set_id(4);
  request.mutable_get_status();
  const auto status = send(api, request).status();
  require(status.machine_model() == carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1,
          "mounted EEPROM should provide the firmware machine model");
  require(status.function_setting() == 1, "mounted EEPROM should provide persistent firmware function flags");

  return 0;
}
