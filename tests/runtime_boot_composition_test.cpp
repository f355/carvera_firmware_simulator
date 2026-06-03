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

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define private public
#define protected public
#include "libs/Kernel.h"
#undef protected
#undef private

#include "Config.h"
#include "Gcode.h"
#include "LaserPublicAccess.h"
#include "PlayerPublicAccess.h"
#include "PublicData.h"
#include "PwmOut.h"
#include "Robot.h"
#include "SpindlePublicAccess.h"
#include "checksumm.h"
#include "lpc17xx_wdt.h"
#include "sim/firmware_runtime.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void require_contains(const std::string& haystack, const char* needle, const char* message) {
  if (haystack.find(needle) == std::string::npos) {
    std::cerr << message << "\nmissing: " << needle << "\nactual:\n" << haystack << '\n';
    std::exit(1);
  }
}

void write_sd_config(const std::filesystem::path& root, const std::string& text) {
  std::filesystem::create_directories(root / "gcodes");
  std::ofstream config(root / "config.txt");
  config << text;
  std::ofstream override_file(root / "config-override");
  override_file << "; simulator runtime override test\n";
  override_file << "M493.2 T1\n";
  std::ofstream job(root / "gcodes" / "runtime-player.cnc");
  job << "G91\n";
  job << "G1 X1 F600\n";
}

void boot_runtime_with_sd(sim::FirmwareRuntime& runtime, const std::filesystem::path& root) {
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);
  (void)runtime.boot();
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_runtime_boot_composition");
  const auto& root = temp_root.path();
  write_sd_config(root,
                  "sd_ok true\n"
                  "spindle.delay_s 0\n"
                  "spindle.alarm_pin nc\n"
                  "spindle.pulses_per_rev 12\n"
                  "spindle.acc_ratio 1.635\n"
                  "spindle.control_smoothing 0.001\n");

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime runtime(simulator);
  boot_runtime_with_sd(runtime, root);
  auto& kernel = runtime.boot();

  const auto boot_output = runtime.read_serial();
  require_contains(boot_output, "version =", "runtime boot should mirror the firmware startup version print");
  require(kernel.is_grbl_mode(), "simulator firmware target should follow the CNC firmware build's GRBL mode default");
  require(MAX_ROBOT_ACTUATORS == 5, "simulator firmware target should follow build/build.sh AXIS=5");
  require(N_PRIMARY_AXIS == 3, "simulator firmware target should follow build/build.sh PAXIS=3");
  require(boot_output.find("G28 means goto clearance position on CARVERA") == std::string::npos,
          "boot homing should use the CNC/GRBL G28.2 path, not the ATC G28 clearance path");

  require(kernel.config->value(get_checksum("sd_ok"))->as_bool(false),
          "runtime config should represent a present simulator SD card through sd_ok");

  runtime.write_serial("M23 runtime-player\nM24\n");
  runtime.run_main_loop(8);
  void* progress_storage = nullptr;
  require(PublicData::get_value(player_checksum, get_progress_checksum, &progress_storage),
          "free-running runtime should load real Player");

  laser_status laser{};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &laser),
          "free-running runtime should load real Laser");

  runtime.write_serial("M321.2\nM323\n");
  runtime.run_until_idle(50'000);
  auto laser_output = runtime.read_serial();
  laser = {};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &laser),
          "runtime Laser status should remain available after mode commands");
  if (!laser.mode || !laser.testing) {
    std::cerr << "M321.2/M323 should switch the runtime Laser into test mode\nserial:\n" << laser_output << '\n';
    return 1;
  }

  runtime.write_serial("M322.2\n");
  runtime.run_until_idle(50'000);
  laser_output = runtime.read_serial();
  laser = {};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &laser),
          "runtime Laser status should remain available after returning to CNC mode");
  if (laser.mode || laser.testing) {
    std::cerr << "M322.2 should return the runtime Laser to CNC mode\nserial:\n" << laser_output << '\n';
    return 1;
  }

  runtime.write_serial("M3 S6000\n");
  runtime.run_until_idle(50'000);
  const auto spindle_output = runtime.read_serial();
  spindle_status spindle{};
  require(PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &spindle),
          "free-running runtime should load the real PWM spindle");
  if (!spindle.state) {
    std::cerr << "M3 should turn on the runtime PWM spindle\nserial:\n" << spindle_output << '\n';
    return 1;
  }

  for (int i = 0; i < 5'000; ++i) {
    simulator.advance_us(1'000);
    runtime.pump_free_running(1, 100);
  }
  spindle = {};
  require(PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &spindle),
          "PWM spindle status should remain available after simulated tach feedback");
  require(spindle.current_rpm > 0.0F, "simulator should feed spindle tach pulses back to the firmware");
  require(std::fabs(spindle.current_rpm - 6'000.0F) < 500.0F,
          "C1 tach feedback should compensate spindle.acc_ratio so firmware RPM matches the physical spindle speed");

  require((LPC_WDT->WDMOD & WDT_WDMOD_WDEN) != 0, "free-running runtime should load the real Watchdog module");
  const auto disabled_gcode_hook_count = kernel.hooks[ON_GCODE_RECEIVED].size();

  runtime.reset();
  write_sd_config(root, "sd_ok true\nspindle.delay_s 0\ndrillingcycles.enable true\ndrillingcycles.dwell_units P\n");
  boot_runtime_with_sd(runtime, root);
  require(runtime.boot().hooks[ON_GCODE_RECEIVED].size() == disabled_gcode_hook_count + 1,
          "free-running runtime should load real Drillingcycles and let its config gate decide whether it registers");

  runtime.reset();
  require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0}),
          "CA1 factory settings should apply before reboot");
  boot_runtime_with_sd(runtime, root);
  auto& ca1_kernel = runtime.boot();
  require(!ca1_kernel.robot->is_soft_endstop_enabled(),
          "runtime should not force CA1 soft endstops when the SD config does not enable them");
  return 0;
}
