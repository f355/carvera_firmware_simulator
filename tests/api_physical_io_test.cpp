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

#include "PwmOut.h"
#include "support/api_service_harness.hpp"
#include "support/assertions.hpp"

int main() {
  using sim::test::require;

  sim::test::ApiHarness api;

  auto response = api.request([](auto& request) {
    request.mutable_set_gpio_input()->mutable_pin()->set_port(1);
    request.mutable_set_gpio_input()->mutable_pin()->set_pin(18);
    request.mutable_set_gpio_input()->set_high(true);
  });
  require(response.ok(), "set_gpio_input should succeed");

  response = api.request([](auto& request) {
    request.mutable_get_gpio_level()->mutable_pin()->set_port(1);
    request.mutable_get_gpio_level()->mutable_pin()->set_pin(18);
  });
  require(response.ok(), "get_gpio_level should succeed");
  require(response.gpio_level().high(), "get_gpio_level should report driven input");

  response = api.request([](auto& request) {
    auto* attach_axis = request.mutable_attach_step_dir_axis();
    attach_axis->mutable_step_pin()->set_port(1);
    attach_axis->mutable_step_pin()->set_pin(18);
    attach_axis->mutable_direction_pin()->set_port(1);
    attach_axis->mutable_direction_pin()->set_pin(20);
  });
  require(response.ok(), "attach_step_dir_axis should succeed");
  require(response.attached_axis().axis() == 0, "first attached axis should be axis zero");

  response = api.request([](auto& request) { request.mutable_get_axis_position()->set_axis(0); });
  require(response.ok(), "get_axis_position should succeed");
  require(response.axis_position().steps() == 0, "new attached axis should start at zero steps");

  mbed::PwmOut pwm(P2_5);
  pwm.period_us(1000.0F);
  pwm.write(0.25F);

  response = api.request([](auto& request) {
    request.mutable_get_pwm_output()->mutable_pin()->set_port(2);
    request.mutable_get_pwm_output()->mutable_pin()->set_pin(5);
  });
  require(response.ok(), "get_pwm_output should succeed");
  require(response.pwm_output().configured(), "get_pwm_output should report configured PWM pins");
  require(response.pwm_output().duty() == 0.25, "get_pwm_output should report PWM duty");
  require(response.pwm_output().period_us() == 1000.0, "get_pwm_output should report PWM period");

  response = api.request([](auto& request) { request.mutable_get_machine_snapshot(); });
  require(response.ok(), "get_machine_snapshot should boot firmware");

  sim::test::pb::PhysicalIoSnapshot io_snapshot;
  require(api.api().fill_physical_io_snapshot_nonblocking(io_snapshot), "physical IO snapshot should fill");
  require(io_snapshot.pwm_outputs_size() == 6, "physical IO snapshot should report all six PWM1 outputs");
  for (int pin = 0; pin <= 5; ++pin) {
    int seen = 0;
    for (const auto& output : io_snapshot.pwm_outputs()) {
      if (output.pin().port() == 2 && static_cast<int>(output.pin().pin()) == pin) {
        ++seen;
      }
    }
    require(seen == 1, "physical IO snapshot should report PWM pin 2." + std::to_string(pin) + " exactly once");
  }

  response = api.request([](auto& request) {
    request.mutable_trigger_interrupt_rise()->mutable_pin()->set_port(2);
    request.mutable_trigger_interrupt_rise()->mutable_pin()->set_pin(6);
  });
  require(response.ok(), "trigger_interrupt_rise should accept valid interrupt pins");

  response = api.request([](auto& request) {
    request.mutable_set_probe_inputs()->set_probe(true);
    request.mutable_set_probe_inputs()->set_tool_setter(true);
  });
  require(response.ok(), "set_probe_inputs should succeed");

  response = api.request([](auto& request) { request.mutable_get_probe_inputs(); });
  require(response.ok(), "get_probe_inputs should succeed");
  require(response.probe_inputs().probe(), "get_probe_inputs should read probe contact through real ZProbe");
  require(response.probe_inputs().tool_setter(),
          "get_probe_inputs should read tool-setter contact through real ZProbe");

  response = api.request([](auto& request) { request.mutable_set_probe_tool_installed()->set_installed(true); });
  require(response.ok(), "set_probe_tool_installed should succeed");

  response = api.request([](auto& request) {
    auto* stock = request.mutable_set_stock_box();
    stock->set_enabled(true);
    stock->mutable_bounds()->set_min_x(-1.0);
    stock->mutable_bounds()->set_min_y(-1.0);
    stock->mutable_bounds()->set_min_z(-2.0);
    stock->mutable_bounds()->set_max_x(1.0);
    stock->mutable_bounds()->set_max_y(1.0);
    stock->mutable_bounds()->set_max_z(-1.0);
  });
  require(response.ok(), "set_stock_box should succeed");

  response = api.request([](auto& request) {
    auto* tool_setter = request.mutable_set_tool_setter_box();
    tool_setter->set_enabled(true);
    tool_setter->mutable_bounds()->set_min_x(100.0);
    tool_setter->mutable_bounds()->set_min_y(100.0);
    tool_setter->mutable_bounds()->set_min_z(-1.0);
    tool_setter->mutable_bounds()->set_max_x(110.0);
    tool_setter->mutable_bounds()->set_max_y(110.0);
    tool_setter->mutable_bounds()->set_max_z(1.0);
  });
  require(response.ok(), "set_tool_setter_box should succeed");

  response = api.request([](auto& request) {
    request.mutable_set_adc_input()->set_channel(3);
    request.mutable_set_adc_input()->set_raw(2048);
  });
  require(response.ok(), "set_adc_input should succeed");

  response = api.request([](auto& request) { request.mutable_get_adc_input()->set_channel(3); });
  require(response.ok(), "get_adc_input should succeed");
  require(response.adc_input().channel() == 3, "get_adc_input should report channel");
  require(response.adc_input().raw() == 2048, "get_adc_input should report raw ADC value");

  response = api.request([](auto& request) {
    request.mutable_set_temperature()->set_sensor(sim::test::pb::TEMPERATURE_SENSOR_SPINDLE);
    request.mutable_set_temperature()->set_celsius(25.0);
  });
  require(response.ok(), "set_temperature should accept a stock spindle sensor temperature");

  response = api.request([](auto& request) { request.mutable_get_adc_input()->set_channel(5); });
  require(response.ok(), "get_adc_input after set_temperature should succeed");
  require(response.adc_input().raw() >= 3890 && response.adc_input().raw() <= 3930,
          "set_temperature should map spindle Celsius to the stock thermistor ADC channel");

  return 0;
}
