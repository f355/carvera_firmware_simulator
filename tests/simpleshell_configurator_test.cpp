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
#include <regex>

#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"
#include "support/cartesian_config.hpp"
#include "support/assertions.hpp"

using sim::test::require;
using sim::test::require_contains;


int main() {
  sim::test::TempDirectory temp_root("carvera_sim_simpleshell_configurator_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.sd_ok = false;
  sim::test::write_cartesian_config(root, config);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  runtime.boot();

  runtime.io().write_serial("version\nmodel\ntime 1234567890\ntime\n");
  runtime.runner().run_main_loop(16);
  auto serial = runtime.io().read_serial();
  require(std::regex_search(serial, std::regex(R"(version = [0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9\-_]*)")),
          "SimpleShell version should satisfy the controller's semantic-version parser");
  require_contains(serial, "model = C1", "SimpleShell model should keep the controller-visible model format");
  require_contains(serial, "time = 1234567890",
                   "SimpleShell time sync should persist through the simulator clock stub");

  runtime.io().write_serial("config-get alpha_steps_per_mm\n");
  runtime.runner().run_main_loop(8);
  serial = runtime.io().read_serial();
  require_contains(serial, "cached: alpha_steps_per_mm is set to 200",
                   "real SimpleShell/Configurator should answer config-get through the serial console");

  runtime.io().write_serial("config-set sd simulator.test_value 20\n");
  runtime.runner().run_main_loop(8);
  serial = runtime.io().read_serial();
  require_contains(serial, "sd: simulator.test_value has been set to 20",
                   "real Configurator should persist config-set to the host-backed SD config");

  runtime.io().write_serial("config-get sd simulator.test_value\n");
  runtime.runner().run_main_loop(8);
  serial = runtime.io().read_serial();
  require_contains(serial, "sd: simulator.test_value is set to 20",
                   "real Configurator should read the updated value from the sd source");

  runtime.io().write_serial("config-delete sd simulator.test_value\n");
  runtime.runner().run_main_loop(8);
  serial = runtime.io().read_serial();
  require_contains(serial, "sd: simulator.test_value has been removed",
                   "real Configurator should remove values from the host-backed SD config");

  runtime.io().write_serial("config-get sd simulator.test_value\n");
  runtime.runner().run_main_loop(8);
  serial = runtime.io().read_serial();
  require_contains(serial, "sd: simulator.test_value is not in config",
                   "real Configurator should report a deleted source value as missing");

  require((runtime.factory_settings().function_setting & 0x01) == 0,
          "test should start with the optional rotary A-axis factory flag disabled");
  runtime.io().write_serial("enable_4th_hd\n");
  runtime.runner().run_main_loop(16);
  serial = runtime.io().read_serial();
  require_contains(serial, "successed! enalbe Harmonic Drive 4th Axis ok!",
                   "real SimpleShell should run the C1 harmonic-drive enable path");
  require((runtime.factory_settings().function_setting & 0x01) != 0,
          "simulator factory settings should reflect real firmware FuncSetting writes");
  runtime.reset();
  require((runtime.factory_settings().function_setting & 0x01) != 0,
          "simulator reset should preserve factory settings written by firmware");
  runtime.boot();
  require((runtime.factory_settings().function_setting & 0x01) != 0,
          "firmware should read the updated factory settings from simulated EEPROM after reboot");
  return 0;
}
