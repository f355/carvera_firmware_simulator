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
#include "PwmOut.h"
#include "SwitchPublicAccess.h"
#include "libs/Kernel.h"
#include "libs/SlowTicker.h"
#include "modules/tools/switch/SwitchPool.h"
#include "sim/machine_simulator.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;
using sim::test::require_near;

pad_switch switch_status(uint16_t name) {
  pad_switch status{};
  require(PublicData::get_value(switch_checksum, name, 0, &status),
          "configured switch should publish its state through PublicData");
  return status;
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  mbed::PwmOut::reset_states();
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "switch.light.enable true\n",
      "switch.light.output_pin 2.0\n",
      "switch.light.output_type digital\n",
      "switch.light.input_on_command M821\n",
      "switch.light.input_off_command M822\n",
      "switch.light.startup_state false\n",
      "switch.vacuum.enable true\n",
      "switch.vacuum.output_pin 2.13\n",
      "switch.vacuum.output_type digitalpwm\n",
      "switch.vacuum.pwm_pin 2.3\n",
      "switch.vacuum.input_on_command M801\n",
      "switch.vacuum.input_off_command M802\n",
      "switch.vacuum.min_pwm 20\n",
      "switch.vacuum.max_pwm 80\n",
      "switch.vacuum.default_on_value 80\n",
      "switch.spindlefan.enable true\n",
      "switch.spindlefan.output_pin 2.1\n",
      "switch.spindlefan.output_type hwpwm\n",
      "switch.spindlefan.input_on_command M811\n",
      "switch.spindlefan.input_off_command M812\n",
      "switch.spindlefan.default_on_value 50\n",
      "switch.extendout.enable true\n",
      "switch.extendout.output_pin 0.29\n",
      "switch.extendout.output_type swpwm\n",
      "switch.extendout.input_on_command M851\n",
      "switch.extendout.input_off_command M852\n",
      "switch.extendout.default_on_value 50\n",
      "switch.extendin.enable true\n",
      "switch.extendin.input_pin 0.21\n",
      "switch.extendin.input_pin_behavior momentary\n",
  }));
  kernel.config->config_cache_load();

  SwitchPool pool;
  pool.load_tools();

  Gcode turn_on("M821", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &turn_on);
  require(simulator.gpio_level({2, 0}), "configured light switch should drive its GPIO high on M821");

  pad_switch pad = switch_status(light_checksum);
  require(pad.state, "PublicData switch state should be on after M821");
  require(pad.value == 100.0F, "PublicData switch value should reflect digital on");

  Gcode turn_off("M822", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &turn_off);
  require(!simulator.gpio_level({2, 0}), "configured light switch should drive its GPIO low on M822");

  pad = switch_status(light_checksum);
  require(!pad.state, "PublicData switch state should be off after M822");
  require(pad.value == 0.0F, "PublicData switch value should reflect digital off");

  Gcode vacuum_on("M801 S90", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &vacuum_on);
  pad = switch_status(vacuum_checksum);
  require(pad.state && pad.value == 90.0F, "vacuum command should publish its requested power");
  require(simulator.gpio_level({2, 13}), "vacuum command should enable its digital power output");
  require_near(mbed::PwmOut::state(P2_3).duty, 0.8, 1.0e-6,
               "vacuum PWM should clamp requested power to the configured maximum");

  Gcode vacuum_off("M802", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &vacuum_off);
  require(!simulator.gpio_level({2, 13}), "vacuum off command should disable its digital power output");
  require_near(mbed::PwmOut::state(P2_3).duty, 0.2, 1.0e-6,
               "vacuum PWM should retain the configured minimum duty while disabled");

  Gcode fan_on("M811", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &fan_on);
  pad = switch_status(spindlefan_checksum);
  require(pad.state && pad.value == 50.0F, "spindle fan should use its configured default duty");
  require_near(mbed::PwmOut::state(P2_1).duty, 0.5, 1.0e-6,
               "spindle fan should drive the hardware PWM output at its default duty");

  Gcode fan_full("M811 S120", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &fan_full);
  require_near(mbed::PwmOut::state(P2_1).duty, 1.0, 1.0e-6,
               "hardware PWM switch should clamp duty requests above 100 percent");

  Gcode fan_off("M812", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &fan_off);
  require_near(mbed::PwmOut::state(P2_1).duty, 0.0, 1.0e-6, "spindle fan off command should clear hardware PWM duty");

  Gcode extend_on("M851 S25", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &extend_on);
  pad = switch_status(extendout_checksum);
  require(pad.state && pad.value == 25.0F,
          "software PWM extension output should publish its requested real-machine duty");

  Gcode extend_off("M852", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &extend_off);
  pad = switch_status(extendout_checksum);
  require(!pad.state && pad.value == 0.0F, "software PWM extension output should turn off cleanly");

  simulator.set_gpio_input({0, 21}, true);
  kernel.slow_ticker->tick();
  kernel.slow_ticker->tick();
  kernel.call_event(ON_MAIN_LOOP);
  pad = switch_status(CHECKSUM("extendin"));
  require(pad.state, "config2 extension input should report a pressed momentary switch");

  simulator.set_gpio_input({0, 21}, false);
  kernel.slow_ticker->tick();
  kernel.slow_ticker->tick();
  kernel.call_event(ON_MAIN_LOOP);
  pad = switch_status(CHECKSUM("extendin"));
  require(!pad.state, "config2 extension input should clear when the momentary switch is released");

  return 0;
}
