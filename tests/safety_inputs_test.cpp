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

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

void verify_cover_input(const char* test_name, sim::MachineModel model, const char* cover_pin) {
  sim::test::TempDirectory temp_root(test_name);
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.extra = std::string("cover_endstop ") + cover_pin + "\nendstop_debounce_count 1\n";
  sim::test::write_cartesian_config(root, config);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  require(runtime.set_factory_settings({model, 2}), "test should configure requested factory model");
  runtime.boot();

  runtime.inputs().set_cover_open(false);
  require(!runtime.inputs().cover_open(), "configured cover input should report closed");
  runtime.inputs().set_cover_open(true);
  require(runtime.inputs().cover_open(), "configured cover input should report open");
}

}  // namespace

int main() {
  verify_cover_input("carvera_sim_c1_cover_input_test", sim::MachineModel::CarveraC1, "1.9^");
  verify_cover_input("carvera_sim_ca1_cover_input_test", sim::MachineModel::CarveraAirCA1, "1.8!^");

  sim::test::TempDirectory temp_root("carvera_sim_safety_inputs_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.extra =
      "alpha_limit_enable true\n"
      "alpha_motor_alarm_pin 0.1!^\n"
      "beta_limit_enable true\n"
      "gamma_limit_enable true\n"
      "cover_endstop 1.8!^\n"
      "endstop_debounce_count 1\n";
  sim::test::write_cartesian_config(root, config);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();

  runtime.inputs().set_cover_open(false);
  require(!runtime.inputs().cover_open(), "configured inverted cover input should report closed");
  runtime.inputs().set_cover_open(true);
  require(runtime.inputs().cover_open(), "configured inverted cover input should report open");

  runtime.inputs().set_limit_switch(0, sim::LimitSwitchSide::Max, false);
  require(!runtime.inputs().limit_switch(0, sim::LimitSwitchSide::Max), "configured X max limit should start released");
  runtime.inputs().set_limit_switch(0, sim::LimitSwitchSide::Max, true);
  require(runtime.inputs().limit_switch(0, sim::LimitSwitchSide::Max),
          "configured X max limit should read through the real endstop pin polarity");

  require(!runtime.inputs().motor_alarm(0), "configured inverted X motor alarm should start inactive");
  runtime.inputs().set_motor_alarm(0, true);
  require(runtime.inputs().motor_alarm(0), "configured inverted X motor alarm should report triggered");
  runtime.runner().run_main_loop(1);
  require(kernel.is_halted(), "real Endstops should halt firmware on a triggered motor alarm");
  require(kernel.get_halt_reason() == MOTOR_ERROR_X, "motor alarm should set the X motor error halt reason");
  return 0;
}
