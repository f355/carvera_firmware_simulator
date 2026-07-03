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
#include "sim/simulation_instance.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void require_contains(const std::string& text, const std::string& needle, const char* message) {
  if (text.find(needle) == std::string::npos) {
    std::cerr << message << "\nstatus was: " << text << '\n';
    std::exit(1);
  }
}

carvera::sim::v1::Response send(sim::ApiService& api, carvera::sim::v1::Request& request) {
  const auto response = api.handle(request);
  require(response.ok(), response.error().empty() ? "API request failed" : response.error().c_str());
  request.Clear();
  return response;
}

void run_until_idle(sim::ApiService& api, carvera::sim::v1::Request& request) {
  request.mutable_run_until_idle()->set_max_step_ticks(20'000);
  (void)send(api, request);
}

std::string serial_status(sim::ApiService& api, carvera::sim::v1::Request& request) {
  request.mutable_write_serial()->set_data("?\n");
  (void)send(api, request);
  run_until_idle(api, request);
  request.mutable_read_serial();
  return send(api, request).serial_data().data();
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_motor_alarm_api_test");
  sim::test::CartesianConfigOptions config;
  config.extra =
      "alpha_limit_enable true\n"
      "alpha_motor_alarm_pin 0.1!^\n"
      "beta_limit_enable true\n"
      "beta_motor_alarm_pin 0.0!^\n"
      "gamma_limit_enable true\n"
      "gamma_motor_alarm_pin 3.25!^\n"
      "spindle.enable true\n"
      "spindle.type pwm\n"
      "spindle.pwm_pin 2.5\n"
      "spindle.feedback_pin 2.7\n"
      "spindle.alarm_pin 0.19^\n"
      "spindle.delay_s 0\n";
  sd.write_config(sim::test::cartesian_config(config));

  sim::SimulationInstance simulation;
  sim::ApiService api(simulation);
  carvera::sim::v1::Request request;

  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  (void)send(api, request);

  request.mutable_get_machine_snapshot();
  auto boot_response = send(api, request);
  require(boot_response.machine_snapshot().homed(), "test runtime should boot and home before fault injection");

  request.mutable_set_motor_alarm()->set_axis(carvera::sim::v1::AXIS_X);
  request.mutable_set_motor_alarm()->set_triggered(true);
  (void)send(api, request);
  run_until_idle(api, request);

  auto status = serial_status(api, request);
  require_contains(status, "<Alarm", "motor alarm should move firmware into Alarm state");
  require_contains(status, "|H:22", "X motor alarm should report halt reason 22");

  request.mutable_write_serial()->set_data("M999\n");
  (void)send(api, request);
  run_until_idle(api, request);
  status = serial_status(api, request);
  require_contains(status, "<Alarm", "M999 should not clear a still-active motor alarm");
  require_contains(status, "|H:22", "still-active X motor alarm should reassert halt reason 22");

  request.mutable_set_motor_alarm()->set_axis(carvera::sim::v1::AXIS_X);
  request.mutable_set_motor_alarm()->set_triggered(false);
  (void)send(api, request);
  request.mutable_write_serial()->set_data("M999\n");
  (void)send(api, request);
  run_until_idle(api, request);
  status = serial_status(api, request);
  require(status.find("<Idle") != std::string::npos, "released motor alarm should recover to Idle after M999");
  require(status.find("|H:22") == std::string::npos, "released motor alarm should clear halt reason from status");

  request.mutable_set_spindle_alarm()->set_triggered(true);
  (void)send(api, request);
  request.mutable_get_spindle_alarm();
  auto response = send(api, request);
  require(response.spindle_alarm_state().available(), "configured spindle alarm should be readable");
  require(response.spindle_alarm_state().triggered(), "spindle alarm readback should report the driven input");
  status = serial_status(api, request);
  require(status.find("<Alarm") != std::string::npos, "spindle alarm should move firmware into Alarm state");
  require(status.find("|H:41") != std::string::npos, "spindle alarm should report halt reason 41");

  request.mutable_set_spindle_alarm()->set_triggered(false);
  (void)send(api, request);
  request.mutable_write_serial()->set_data("M999\n");
  (void)send(api, request);
  run_until_idle(api, request);
  status = serial_status(api, request);
  require(status.find("<Idle") != std::string::npos, "released spindle alarm should recover to Idle after M999");
  require(status.find("|H:41") == std::string::npos, "released spindle alarm should clear halt reason from status");

  return 0;
}
