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

#include "support/booted_runtime.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void require_contains(const std::string& haystack, const std::string& needle, const char* message) {
  if (haystack.find(needle) == std::string::npos) {
    std::cerr << message << "\nmissing: " << needle << "\noutput:\n" << haystack << '\n';
    std::exit(1);
  }
}

void pump_temperature(sim::FirmwareRuntime& runtime) {
  for (int i = 0; i < 120; ++i) {
    runtime.pump_free_running(8, 1'000);
  }
}

std::string temperature_config(const std::string& extra) {
  sim::test::CartesianConfigOptions options;
  options.extra = extra;
  return sim::test::cartesian_config(options);
}

}  // namespace

int main() {
  {
    sim::test::TempSdCard sd("carvera_sim_spindle_temperature_reporting_test");
    sd.write_config(
        temperature_config("temperature_control.spindle.enable true\n"
                           "temperature_control.spindle.thermistor_pin 1.31\n"
                           "temperature_control.spindle.heater_pin nc\n"
                           "temperature_control.spindle.beta 3950\n"
                           "temperature_control.spindle.max_temp 90\n"
                           "temperature_control.spindle.get_m_code 105\n"
                           "temperature_control.spindle.designator M\n"));
    sd.mount();

    sim::test::BootedRuntime booted;
    auto& runtime = booted.runtime();
    auto& kernel = booted.kernel();
    require(!kernel.is_halted(), "runtime should boot before spindle temperature reporting");
    (void)runtime.read_serial();

    runtime.set_temperature(sim::TemperatureSensor::Spindle, 42.0);
    pump_temperature(runtime);
    runtime.write_serial("M105\n");
    require(runtime.run_until_idle(20'000), "M105 should be handled without queued motion");
    require_contains(runtime.read_serial(), "M:42.", "M105 should report the spindle thermistor through firmware");
  }

  {
    sim::test::TempSdCard sd("carvera_sim_power_temperature_reporting_test");
    sd.write_config(
        temperature_config("temperature_control.power.enable true\n"
                           "temperature_control.power.thermistor_pin 0.26\n"
                           "temperature_control.power.heater_pin nc\n"
                           "temperature_control.power.beta 3950\n"
                           "temperature_control.power.max_temp 90\n"
                           "temperature_control.power.get_m_code 106\n"
                           "temperature_control.power.designator P\n"));
    sd.mount();

    sim::test::BootedRuntime booted(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0});
    auto& runtime = booted.runtime();
    auto& kernel = booted.kernel();
    require(!kernel.is_halted(), "runtime should boot before power temperature reporting");
    (void)runtime.read_serial();

    runtime.set_temperature(sim::TemperatureSensor::Power, 45.0);
    pump_temperature(runtime);
    runtime.write_serial("M106\n");
    require(runtime.run_until_idle(20'000), "M106 should be handled without queued motion");
    require_contains(runtime.read_serial(), "P:45.", "M106 should report the CA1 power thermistor through firmware");
  }

  return 0;
}
