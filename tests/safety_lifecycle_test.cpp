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

#include "Config.h"
#include "ConfigValue.h"
#include "PlayerPublicAccess.h"
#include "PublicData.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "sim/firmware_runtime.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "support/cartesian_config.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

bool player_is_playing() {
  void* value = nullptr;
  return PublicData::get_value(player_checksum, is_playing_checksum, &value) && value != nullptr &&
         *static_cast<bool*>(value);
}

void require_state(bool condition, const char* message, sim::FirmwareRuntime& runtime, Kernel& kernel) {
  if (!condition) {
    std::cerr << message << ": cover_open=" << runtime.cover_open() << ", playing=" << player_is_playing()
              << ", state=" << static_cast<int>(kernel.get_state())
              << ", halt_reason=" << static_cast<int>(kernel.get_halt_reason()) << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "carvera_sim_safety_lifecycle_test";
  std::filesystem::remove_all(root);

  sim::test::CartesianConfigOptions config;
  config.extra =
      "alpha_limit_enable true\n"
      "alpha_motor_alarm_pin 0.1!^\n"
      "beta_limit_enable true\n"
      "beta_motor_alarm_pin 0.0!^\n"
      "gamma_limit_enable true\n"
      "gamma_motor_alarm_pin 3.25!^\n"
      "cover_endstop 1.8!^\n"
      "stop_on_cover_open true\n"
      "main_button_pin 1.16^\n"
      "main_button_poll_frequency 20\n"
      "main_button_long_press_time 3000\n"
      "main_button_long_press_enable None\n";
  sim::test::write_cartesian_config(root, config);
  std::filesystem::create_directories(root / "gcodes");
  {
    std::ofstream job(root / "gcodes" / "cover.cnc");
    for (int i = 0; i < 5000; ++i) {
      job << "G4 P0\n";
    }
  }
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime runtime(simulator);
  auto& kernel = runtime.boot();
  require(kernel.config->value(get_checksum("stop_on_cover_open"))->as_bool(false),
          "test config should enable stop_on_cover_open");

  runtime.write_serial("M23 cover\n");
  runtime.write_serial("M24\n");
  runtime.run_until_idle(20'000);
  require(player_is_playing(), "test job should be playing before cover is opened");

  runtime.set_cover_open(true);
  for (int i = 0; i < 20 && !kernel.is_halted(); ++i) {
    runtime.pump_free_running(8, 20'000);
  }

  require_state(kernel.is_halted(), "opening the cover during Player playback should halt the firmware", runtime,
                kernel);
  require(kernel.get_halt_reason() == COVER_OPEN, "cover-open policy should report COVER_OPEN");
  require(!player_is_playing(), "Player should abort the active job when cover-open halt fires");

  runtime.set_cover_open(false);
  runtime.write_serial("M999\n");
  runtime.run_until_idle(20'000);
  require(!kernel.is_halted(), "M999 should clear the cover-open alarm after the cover is closed");
  require(!player_is_playing(), "M999 recovery should not restart an aborted Player job");

  runtime.set_e_stop_pressed(true);
  runtime.run_main_loop(1);
  require(kernel.is_halted(), "pressing e-stop should halt the firmware");
  require(kernel.get_halt_reason() == E_STOP, "e-stop policy should report E_STOP");

  runtime.write_wifi_tcp("M999\n");
  runtime.run_until_idle(20'000);
  require(kernel.is_halted(), "WiFi M999 should not clear alarm while e-stop remains pressed");
  require(kernel.get_halt_reason() == E_STOP, "still-pressed e-stop should reassert E_STOP");

  runtime.set_e_stop_pressed(false);
  runtime.write_wifi_tcp("M999\n");
  runtime.run_until_idle(20'000);
  require(!kernel.is_halted(), "WiFi M999 should clear e-stop alarm after the physical switch is released");

  runtime.set_motor_alarm(1, true);
  runtime.run_main_loop(1);
  require(kernel.is_halted(), "triggering Y motor alarm should halt the firmware");
  require(kernel.get_halt_reason() == MOTOR_ERROR_Y, "Y motor alarm should report MOTOR_ERROR_Y");
  const auto motor_alarm_status = kernel.get_query_string();
  require(
      motor_alarm_status.find("<Alarm") != std::string::npos && motor_alarm_status.find("|H:23") != std::string::npos,
      "controller status should expose the motor alarm halt reason");

  runtime.write_serial("M999\n");
  runtime.run_until_idle(20'000);
  require(kernel.is_halted(), "M999 should not clear a still-active motor alarm");
  require(kernel.get_halt_reason() == MOTOR_ERROR_Y, "still-active Y motor alarm should reassert MOTOR_ERROR_Y");

  runtime.set_motor_alarm(1, false);
  runtime.write_serial("M999\n");
  runtime.run_until_idle(20'000);
  require(!kernel.is_halted(), "M999 should clear motor alarm after the physical alarm input is released");

  std::filesystem::remove_all(root);
  return 0;
}
