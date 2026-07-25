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

#include <string>

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

void pump_temperature_ticks(sim::FirmwareRuntime& runtime) {
  for (int i = 0; i < 120 && !runtime.boot().is_halted(); ++i) {
    runtime.runner().pump_free_running(8, 1'000);
  }
}

std::string spindle_temperature_config() {
  sim::test::CartesianConfigOptions options;
  options.extra =
      "temperature_control.spindle.enable true\n"
      "temperature_control.spindle.thermistor_pin 1.31\n"
      "temperature_control.spindle.heater_pin nc\n"
      "temperature_control.spindle.beta 3950\n"
      "temperature_control.spindle.max_temp 60\n"
      "temperature_control.spindle.get_m_code 105\n"
      "temperature_control.spindle.designator M\n";
  return sim::test::cartesian_config(options);
}

std::string power_temperature_config() {
  sim::test::CartesianConfigOptions options;
  options.extra =
      "temperature_control.power.enable true\n"
      "temperature_control.power.thermistor_pin 0.26\n"
      "temperature_control.power.heater_pin nc\n"
      "temperature_control.power.beta 3950\n"
      "temperature_control.power.max_temp 60\n"
      "temperature_control.power.get_m_code 106\n"
      "temperature_control.power.designator P\n";
  return sim::test::cartesian_config(options);
}

}  // namespace

int main() {
  {
    sim::test::TempSdCard sd("carvera_sim_spindle_overtemp_runtime_test");
    sd.write_config(spindle_temperature_config());
    sim::SimulationInstance simulation(sd.persistent_config());
    auto& runtime = simulation.firmware();
    auto& kernel = runtime.boot();
    require(!kernel.is_halted(), "runtime should boot before spindle temperature fault injection");

    runtime.inputs().set_temperature(sim::TemperatureSensor::Spindle, 80.0);
    pump_temperature_ticks(runtime);

    require(kernel.is_halted(), "spindle over-temperature should halt through real TemperatureControl");
    require(kernel.get_halt_reason() == SPINDLE_OVERHEATED,
            "spindle over-temperature should report the firmware spindle fault reason");
    require(runtime.io().read_serial_text().find("Spindle overheated") != std::string::npos,
            "spindle over-temperature should print the firmware error");
  }

  {
    sim::test::TempSdCard sd("carvera_sim_power_overtemp_runtime_test");
    sd.write_config(power_temperature_config());
    sim::SimulationInstance simulation(sd.persistent_config());
    auto& runtime = simulation.firmware();
    require(runtime.set_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0}),
            "CA1 factory settings should apply before boot");
    auto& kernel = runtime.boot();
    require(!kernel.is_halted(), "runtime should boot before power temperature fault injection");

    runtime.inputs().set_temperature(sim::TemperatureSensor::Power, 80.0);
    pump_temperature_ticks(runtime);

    require(kernel.is_halted(), "power-cabinet over-temperature should halt through real TemperatureControl");
    require(kernel.get_halt_reason() == POWER_OVERHEATED,
            "power-cabinet over-temperature should report the firmware power fault reason");
    require(runtime.io().read_serial_text().find("Power cabinet overheated") != std::string::npos,
            "power-cabinet over-temperature should print the firmware error");
  }

  return 0;
}
