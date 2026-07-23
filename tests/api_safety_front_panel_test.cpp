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

#include "sim/makera_protocol.hpp"
#include "support/api_service_harness.hpp"
#include "support/api_snapshot_config.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"

int main() {
  using sim::test::require;

  sim::makera::FrameDecoder serial_decoder;
  sim::test::TempSdCard sd("carvera_sim_api_safety_test");
  sim::test::write_api_snapshot_config(sd.path());
  sim::test::ApiHarness api(sd.persistent_config());
  auto response = api.request([](auto& request) { request.mutable_set_cover_open()->set_open(true); });
  require(response.ok(), "set_cover_open should succeed");

  response = api.request([](auto& request) { request.mutable_get_cover_open(); });
  require(response.ok(), "get_cover_open should succeed");
  require(response.cover_state().open(), "get_cover_open should read the configured cover input through Endstops");

  response = api.request([](auto& request) {
    request.mutable_set_limit_switch()->set_axis(sim::test::pb::AXIS_X);
    request.mutable_set_limit_switch()->set_side(sim::test::pb::LIMIT_SWITCH_SIDE_MIN);
    request.mutable_set_limit_switch()->set_triggered(true);
  });
  require(response.ok(), "set_limit_switch should succeed");

  response = api.request([](auto& request) {
    request.mutable_get_limit_switch()->set_axis(sim::test::pb::AXIS_X);
    request.mutable_get_limit_switch()->set_side(sim::test::pb::LIMIT_SWITCH_SIDE_MIN);
  });
  require(response.ok(), "get_limit_switch should succeed");
  require(response.limit_switch_state().axis() == sim::test::pb::AXIS_X,
          "get_limit_switch should echo the requested axis");
  require(response.limit_switch_state().side() == sim::test::pb::LIMIT_SWITCH_SIDE_MIN,
          "get_limit_switch should echo the requested side");
  require(response.limit_switch_state().triggered(), "get_limit_switch should read configured limit pin polarity");

  response = api.request([](auto& request) {
    request.mutable_set_motor_alarm()->set_axis(sim::test::pb::AXIS_X);
    request.mutable_set_motor_alarm()->set_triggered(true);
  });
  require(response.ok(), "set_motor_alarm should succeed");

  response = api.request([](auto& request) { request.mutable_get_motor_alarm()->set_axis(sim::test::pb::AXIS_X); });
  require(response.ok(), "get_motor_alarm should succeed");
  require(response.motor_alarm_state().axis() == sim::test::pb::AXIS_X,
          "get_motor_alarm should echo the requested axis");
  require(response.motor_alarm_state().triggered(), "get_motor_alarm should read configured motor alarm polarity");

  response = api.request([](auto& request) { request.mutable_get_front_panel_state(); });
  require(response.ok(), "get_front_panel_state should succeed");
  require(response.front_panel_state().power_rails().v12(), "front panel state should expose the 12V rail");
  require(response.front_panel_state().power_rails().v24(), "front panel state should expose the 24V rail");

  response = api.request([](auto& request) { request.mutable_set_main_button_pressed()->set_pressed(true); });
  require(response.ok(), "set_main_button_pressed should succeed");

  response = api.request([](auto& request) { request.mutable_get_front_panel_state(); });
  require(response.ok(), "get_front_panel_state after main-button press should succeed");
  require(response.front_panel_state().main_button_pressed(),
          "front panel state should read the configured main-button pin");

  response = api.request([](auto& request) { request.mutable_set_e_stop_pressed()->set_pressed(true); });
  require(response.ok(), "set_e_stop_pressed should succeed");

  response = api.request(
      [](auto& request) { request.mutable_write_serial()->set_data(sim::makera::encode_console_input("?")); });
  require(response.ok(), "status query after e-stop should enqueue");

  response = api.request([](auto& request) { request.mutable_run_until_idle()->set_max_step_ticks(0); });
  require(response.ok(), "status query after e-stop should run");

  response = api.request([](auto& request) { request.mutable_read_serial(); });
  require(response.ok(), "status query after e-stop should be readable");
  serial_decoder.append(response.serial_data().data());
  auto serial_text = serial_decoder.take_text();
  (void)serial_decoder.take_frames();
  require(serial_text.find("<Alarm") != std::string::npos,
          "pressing e-stop through the API should move firmware into Alarm state");

  response = api.request([](auto& request) { request.mutable_get_front_panel_state(); });
  require(response.ok(), "get_front_panel_state after e-stop press should succeed");
  require(response.front_panel_state().e_stop_pressed(), "front panel state should read the configured e-stop pin");

  return 0;
}
