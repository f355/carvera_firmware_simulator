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
#include "Robot.h"
#include "StepperMotor.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "libs/utils.h"
#include "sim/simulation_instance.hpp"
#include "support/temp_sdcard.hpp"
#include "support/cartesian_config.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

bool player_is_playing() {
  void* value = nullptr;
  return PublicData::get_value(player_checksum, is_playing_checksum, &value) && value != nullptr &&
         *static_cast<bool*>(value);
}

void require_state(bool condition, const char* message, sim::FirmwareRuntime& runtime, Kernel& kernel) {
  if (!condition) {
    std::cerr << message << ": cover_open=" << runtime.inputs().cover_open() << ", playing=" << player_is_playing()
              << ", state=" << static_cast<int>(kernel.get_state())
              << ", halt_reason=" << static_cast<int>(kernel.get_halt_reason()) << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_safety_lifecycle_test");
  const auto& root = temp_root.path();

  sim::test::CartesianConfigOptions config;
  config.extra =
      "home_on_boot false\n"
      "alpha_limit_enable true\n"
      "alpha_motor_alarm_pin 0.1!^\n"
      "beta_limit_enable true\n"
      "beta_motor_alarm_pin 0.0!^\n"
      "gamma_limit_enable true\n"
      "gamma_motor_alarm_pin 3.25!^\n"
      "cover_endstop 1.9^\n"
      "stop_on_cover_open true\n"
      "main_button_pin 1.16^\n"
      "main_button_poll_frequency 20\n"
      "main_button_long_press_time 3000\n"
      "main_button_long_press_enable None\n";
  sim::test::write_cartesian_config(root, config);
  std::filesystem::create_directories(root / "gcodes");
  {
    std::ofstream job(root / "gcodes" / "cover.cnc");
    // Newer firmware only enforces stop_on_cover_open while the spindle is on
    // or an axis is moving, so keep a tiny oscillating move active.
    job << "G91\n";
    for (int i = 0; i < 2500; ++i) {
      job << "G1 X0.01 F300\n";
      job << "G1 X-0.01 F300\n";
    }
  }
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();
  require(kernel.config->value(get_checksum("stop_on_cover_open"))->as_bool(false),
          "test config should enable stop_on_cover_open");

  // Newer firmware treats an un-homed machine as a halt condition. This fixture
  // only needs Player playback + cover policy, so bypass the homed gate and
  // clear any HOME_FAIL raised during the automatic boot pump.
  require(kernel.robot != nullptr, "boot should install Robot");
  kernel.robot->override_homed_check(true);
  if (kernel.is_halted()) {
    runtime.io().write_serial("M999\n");
    runtime.runner().run_main_loop(16);
  }
  require(!kernel.is_halted(), "test should clear boot alarms before Player cover check");

  // Ensure the cover starts closed; unconfigured GPIO can otherwise read open and
  // immediately trip the motion-aware cover policy once the job starts moving.
  runtime.inputs().set_cover_open(false);
  require(!runtime.inputs().cover_open(), "cover should start closed");

  runtime.io().write_serial("M23 cover\n");
  runtime.runner().run_main_loop(8);
  runtime.io().write_serial("M24\n");
  bool playing = false;
  bool moving = false;
  for (int i = 0; i < 200; ++i) {
    runtime.runner().pump_free_running(4, 20'000);
    playing = player_is_playing();
    moving = kernel.robot->actuators[0] != nullptr && kernel.robot->actuators[0]->is_moving();
    if (playing && moving) {
      break;
    }
  }
  require(playing, "test job should be playing before cover is opened");
  require(moving, "test job should be moving before cover is opened");

  runtime.inputs().set_cover_open(true);
  for (int i = 0; i < 20 && !kernel.is_halted(); ++i) {
    runtime.runner().pump_free_running(8, 20'000);
  }

  require_state(kernel.is_halted(), "opening the cover during Player playback should halt the firmware", runtime,
                kernel);
  require(kernel.get_halt_reason() == COVER_OPEN, "cover-open policy should report COVER_OPEN");
  require(!player_is_playing(), "Player should abort the active job when cover-open halt fires");

  runtime.inputs().set_cover_open(false);
  runtime.io().write_serial("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  require(!kernel.is_halted(), "M999 should clear the cover-open alarm after the cover is closed");
  require(!player_is_playing(), "M999 recovery should not restart an aborted Player job");

  runtime.inputs().set_e_stop_pressed(true);
  runtime.runner().run_main_loop(1);
  require(kernel.is_halted(), "pressing e-stop should halt the firmware");
  require(kernel.get_halt_reason() == E_STOP, "e-stop policy should report E_STOP");

  runtime.io().write_wifi_tcp("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  require(kernel.is_halted(), "WiFi M999 should not clear alarm while e-stop remains pressed");
  require(kernel.get_halt_reason() == E_STOP, "still-pressed e-stop should reassert E_STOP");

  runtime.inputs().set_e_stop_pressed(false);
  runtime.io().write_wifi_tcp("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  require(!kernel.is_halted(), "WiFi M999 should clear e-stop alarm after the physical switch is released");

  runtime.inputs().set_motor_alarm(1, true);
  runtime.runner().run_main_loop(1);
  require(kernel.is_halted(), "triggering Y motor alarm should halt the firmware");
  require(kernel.get_halt_reason() == MOTOR_ERROR_Y, "Y motor alarm should report MOTOR_ERROR_Y");
  const auto motor_alarm_status = kernel.get_query_string();
  require(
      motor_alarm_status.find("<Alarm") != std::string::npos && motor_alarm_status.find("|H:23") != std::string::npos,
      "controller status should expose the motor alarm halt reason");

  runtime.io().write_serial("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  require(kernel.is_halted(), "M999 should not clear a still-active motor alarm");
  require(kernel.get_halt_reason() == MOTOR_ERROR_Y, "still-active Y motor alarm should reassert MOTOR_ERROR_Y");

  runtime.inputs().set_motor_alarm(1, false);
  runtime.io().write_serial("M999\n");
  runtime.runner().run_until_motion_idle(20'000);
  require(!kernel.is_halted(), "M999 should clear motor alarm after the physical alarm input is released");
  return 0;
}
