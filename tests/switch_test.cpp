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
#include "SwitchPublicAccess.h"
#include "libs/Kernel.h"
#include "modules/tools/switch/SwitchPool.h"
#include "sim/machine_simulator.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "switch.light.enable true\n",
      "switch.light.output_pin 2.0\n",
      "switch.light.output_type digital\n",
      "switch.light.input_on_command M821\n",
      "switch.light.input_off_command M822\n",
      "switch.light.startup_state false\n",
  }));
  kernel.config->config_cache_load();

  SwitchPool pool;
  pool.load_tools();

  Gcode turn_on("M821", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &turn_on);
  require(simulator.gpio_level({2, 0}), "configured light switch should drive its GPIO high on M821");

  pad_switch pad{};
  require(PublicData::get_value(switch_checksum, light_checksum, 0, &pad),
          "switch should publish its state through PublicData");
  require(pad.state, "PublicData switch state should be on after M821");
  require(pad.value == 100.0F, "PublicData switch value should reflect digital on");

  Gcode turn_off("M822", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &turn_off);
  require(!simulator.gpio_level({2, 0}), "configured light switch should drive its GPIO low on M822");

  pad = {};
  require(PublicData::get_value(switch_checksum, light_checksum, 0, &pad),
          "switch should continue publishing its state after M822");
  require(!pad.state, "PublicData switch state should be off after M822");
  require(pad.value == 0.0F, "PublicData switch value should reflect digital off");

  return 0;
}
