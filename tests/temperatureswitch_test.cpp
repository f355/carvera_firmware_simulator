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

#include "support/assertions.hpp"
#include "support/memory_config.hpp"

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

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.factory_set->MachineModel = CARVERA;
  kernel.set_laser_mode(true);
  kernel.config = new Config(new MemoryConfigSource({
      "switch.spindlefan.enable true\n",
      "switch.spindlefan.output_pin 2.0\n",
      "switch.spindlefan.output_type digital\n",
      "switch.spindlefan.startup_state false\n",
      "temperature_control.spindle.enable true\n",
      "temperature_control.spindle.thermistor_pin 1.31\n",
      "temperature_control.spindle.heater_pin nc\n",
      "temperature_control.spindle.beta 3950\n",
      "temperatureswitch.spindle.enable true\n",
      "temperatureswitch.spindle.switch spindlefan\n",
      "temperatureswitch.spindle.threshold_temp 40.0\n",
      "temperatureswitch.spindle.cooldown_power_init 50.0\n",
      "temperatureswitch.spindle.cooldown_power_step 1.0\n",
      "temperatureswitch.spindle.cooldown_power_laser 80.0\n",
  }));
  kernel.config->config_cache_load();

  SwitchPool pool;
  pool.load_tools();

  simulator.set_adc_channel_raw(5, 1200);
  TemperatureControlPool temperature_pool;
  temperature_pool.load_tools();
  for (int i = 0; i < 6; ++i) {
    TIMER2_IRQHandler();
  }

  TemperatureSwitch loader;
  TemperatureSwitch* temperature_switch = loader.load_config(CHECKSUM("spindle"));
  require(temperature_switch != nullptr, "TemperatureSwitch should load the configured spindle switch");

  kernel.set_laser_mode(false);
  temperature_switch->on_second_tick(nullptr);

  pad_switch pad{};
  require(PublicData::get_value(switch_checksum, spindlefan_checksum, 0, &pad),
          "spindlefan should publish switch state");
  require(pad.state, "temperature above threshold should turn on the configured spindle fan");
  require(simulator.gpio_level({2, 0}), "temperature threshold switch should drive the spindle fan GPIO high");

  kernel.set_laser_mode(true);
  temperature_switch->on_second_tick(nullptr);
  require(PublicData::get_value(switch_checksum, spindlefan_checksum, 0, &pad),
          "spindlefan should still publish switch state");
  require(pad.state, "laser mode should also force the configured spindle fan on");

  delete temperature_switch;
  return 0;
}
