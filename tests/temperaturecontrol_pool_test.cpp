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

#include "test_support.hpp"

#include "Config.h"
#include "Gcode.h"
#include "PublicData.h"
#include "TemperatureControlPool.h"
#include "TemperatureControlPublicAccess.h"
#include "libs/Kernel.h"
#include "lpc1768_sim.h"
#include "sim/machine_simulator.hpp"

extern "C" void TIMER2_IRQHandler(void);

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;
using sim::test::require_near;

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "temperature_control.spindle.enable true\n",
      "temperature_control.spindle.thermistor_pin 1.31\n",
      "temperature_control.spindle.heater_pin nc\n",
      "temperature_control.spindle.beta 3950\n",
      "temperature_control.spindle.get_m_code 105\n",
      "temperature_control.spindle.designator M\n",
  }));
  kernel.config->config_cache_load();

  simulator.set_adc_channel_raw(5, 3911);

  TemperatureControlPool pool;
  pool.load_tools();
  for (int i = 0; i < 6; ++i) {
    TIMER2_IRQHandler();
  }

  pad_temperature temperature{};
  require(PublicData::get_value(temperature_control_checksum, current_temperature_checksum,
                                spindle_temperature_checksum, &temperature),
          "TemperatureControlPool should publish enabled spindle thermistor data");
  require_near(temperature.current_temperature, 25.0F, 0.8F,
               "spindle thermistor should report simulated room temperature through PublicData");

  simulator.set_temperature(sim::TemperatureSensor::Spindle, 42.0);
  for (int i = 0; i < 6; ++i) {
    TIMER2_IRQHandler();
  }
  Gcode read_spindle_temperature("M105", kernel.streams, true, 1);
  kernel.call_event(ON_GCODE_RECEIVED, &read_spindle_temperature);
  require(read_spindle_temperature.txt_after_ok.find("M:42.") != std::string::npos,
          "M105 should report the simulator-set spindle temperature through real TemperatureControl");

  return 0;
}
