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
#include "PublicData.h"
#include "SwitchPublicAccess.h"
#include "TemperatureControlPool.h"
#include "TemperatureControlPublicAccess.h"
#include "libs/Kernel.h"
#include "modules/tools/switch/SwitchPool.h"
#include "modules/tools/temperatureswitch/TemperatureSwitch.h"
#include "sim/machine_simulator.hpp"

extern "C" void TIMER2_IRQHandler(void);

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

void sample_temperatures(int ticks = 6) {
  for (int i = 0; i < ticks; ++i) {
    TIMER2_IRQHandler();
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.factory_set->MachineModel = CARVERA_AIR;
  kernel.config = new Config(new MemoryConfigSource({
      "switch.powerfan.enable true\n",
      "switch.powerfan.output_pin 2.3\n",
      "switch.powerfan.output_type digital\n",
      "switch.powerfan.startup_state false\n",
      "temperature_control.power.enable true\n",
      "temperature_control.power.thermistor_pin 0.26\n",
      "temperature_control.power.heater_pin nc\n",
      "temperature_control.power.beta 3950\n",
      "temperature_control.power.get_m_code 106\n",
      "temperature_control.power.designator P\n",
      "temperatureswitch.power.enable true\n",
      "temperatureswitch.power.switch powerfan\n",
      "temperatureswitch.power.threshold_temp 60.0\n",
      "temperatureswitch.power.cooldown_power_init 50.0\n",
      "temperatureswitch.power.cooldown_power_step 1.0\n",
      "temperatureswitch.power.cooldown_delay 0\n",
  }));
  kernel.config->config_cache_load();

  SwitchPool switch_pool;
  switch_pool.load_tools();

  simulator.set_temperature(sim::TemperatureSensor::Power, 65.0);
  TemperatureControlPool temperature_pool;
  temperature_pool.load_tools();
  sample_temperatures(120);

  TemperatureSwitch loader;
  TemperatureSwitch* temperature_switch = loader.load_config(CHECKSUM("power"));
  require(temperature_switch != nullptr, "CA1 power TemperatureSwitch should load");

  temperature_switch->on_second_tick(nullptr);

  pad_switch pad{};
  require(PublicData::get_value(switch_checksum, powerfan_checksum, 0, &pad), "powerfan should publish switch state");
  require(pad.state, "CA1 power temperature above threshold should turn on the power fan");
  require(simulator.gpio_level({2, 3}), "CA1 power fan switch should drive its configured GPIO high");

  simulator.set_temperature(sim::TemperatureSensor::Power, 25.0);
  sample_temperatures(120);
  temperature_switch->on_second_tick(nullptr);
  require(PublicData::get_value(switch_checksum, powerfan_checksum, 0, &pad), "powerfan should stay readable");
  require(pad.state, "CA1 power fan should stay on for the first cooldown tick below threshold");

  temperature_switch->on_second_tick(nullptr);
  require(PublicData::get_value(switch_checksum, powerfan_checksum, 0, &pad),
          "powerfan should publish cooldown-off state");
  require(!pad.state, "CA1 power fan should turn off after cooldown_delay expires");
  require(!simulator.gpio_level({2, 3}), "CA1 power fan GPIO should go low after cooldown_delay expires");

  delete temperature_switch;
  return 0;
}
