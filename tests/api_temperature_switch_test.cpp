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
#include <fstream>

#include "support/api_service_harness.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void write_temperature_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "sd_ok true\n"
         << "switch.powerfan.enable true\n"
         << "switch.powerfan.output_pin 2.3\n"
         << "switch.powerfan.output_type hwpwm\n"
         << "switch.powerfan.default_on_value 30\n"
         << "temperature_control.power.enable true\n"
         << "temperature_control.power.thermistor_pin 0.26\n"
         << "temperature_control.power.heater_pin nc\n"
         << "temperature_control.power.beta 3950\n"
         << "temperature_control.power.get_m_code 106\n"
         << "temperature_control.power.designator P\n"
         << "temperature_control.power.max_temp 100\n"
         << "temperatureswitch.power.enable true\n"
         << "temperatureswitch.power.switch powerfan\n"
         << "temperatureswitch.power.threshold_temp 20.0\n"
         << "temperatureswitch.power.cooldown_power_init 20.0\n"
         << "temperatureswitch.power.cooldown_power_step 2.5\n"
         << "temperatureswitch.power.cooldown_delay 0\n";
}

}  // namespace

int main() {
  using sim::test::require;

  sim::test::TempSdCard sd("carvera_sim_api_temperature_test");
  write_temperature_config(sd.path());
  sd.mount();

  sim::test::ApiHarness api;
  auto response = api.request([](auto& request) {
    request.mutable_set_machine_model()->set_machine_model(sim::test::pb::MACHINE_MODEL_CARVERA_AIR_CA1);
    request.mutable_set_machine_model()->set_function_setting(0);
  });
  require(response.ok(), "temperature API model setup should succeed before boot");

  response = api.request([](auto& request) {
    request.mutable_set_temperature()->set_sensor(sim::test::pb::TEMPERATURE_SENSOR_POWER);
    request.mutable_set_temperature()->set_celsius(45.0);
  });
  require(response.ok(), "set_temperature should service real firmware temperature switches");

  response = api.request(
      [](auto& request) { request.mutable_get_switch_state()->set_name(sim::test::pb::SWITCH_NAME_POWER_FAN); });
  require(response.ok(), "get_switch_state should read the temperature-driven power fan");
  require(response.switch_state().available(), "temperature-driven power fan should be available");
  require(response.switch_state().on(), "temperature above threshold should turn on the power fan switch");

  response = api.request([](auto& request) {
    request.mutable_get_pwm_output()->mutable_pin()->set_port(2);
    request.mutable_get_pwm_output()->mutable_pin()->set_pin(3);
  });
  require(response.ok(), "get_pwm_output should read the temperature-driven power fan PWM");
  require(response.pwm_output().configured(), "temperature-driven hwpwm fan should configure its PWM output");
  require(response.pwm_output().duty() > 0.0, "temperature-driven fan PWM should reflect the switch value");

  return 0;
}
