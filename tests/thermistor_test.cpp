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

#include "Config.h"
#include "Thermistor.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/persistent_machine_state.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void require_near(float actual, float expected, float tolerance, const char* message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    std::exit(1);
  }
}

void write_temperature_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "temperature_control.spindle.enable true\n"
         << "temperature_control.spindle.thermistor_pin 1.31\n"
         << "temperature_control.spindle.beta 3950\n";
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_thermistor_test");
  const auto& root = temp_root.path();
  write_temperature_config(root);
  sim::PersistentMachineState persistent_state(sim::test::persistent_sd_config(root));
  sim::MachineSimulator simulator(persistent_state);
  Kernel kernel;

  Thermistor thermistor;
  thermistor.UpdateConfig(CHECKSUM("temperature_control"), CHECKSUM("spindle"));

  simulator.set_adc_channel_raw(5, 3911);
  float temperature = 0.0F;
  for (int i = 0; i < 4; ++i) {
    temperature = thermistor.get_temperature();
  }
  require_near(temperature, 25.0F, 0.8F, "stock beta thermistor should convert ADC channel 5 near room temperature");

  simulator.set_temperature(sim::TemperatureSensor::Spindle, 55.0);
  for (int i = 0; i < 4; ++i) {
    temperature = thermistor.get_temperature();
  }
  require_near(temperature, 55.0F, 1.2F, "domain temperature helper should feed real stock thermistor conversion");

  simulator.set_adc_channel_raw(5, 0);
  for (int i = 0; i < 4; ++i) {
    temperature = thermistor.get_temperature();
  }
  require(std::isinf(temperature), "zero ADC should report undefined thermistor temperature");
  return 0;
}
