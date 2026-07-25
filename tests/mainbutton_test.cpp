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
#include "MainButtonPublicAccess.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "modules/utils/mainbutton/MainButton.h"
#include "sim/machine_simulator.hpp"
#include "sim/persistent_machine_state.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::PersistentMachineState persistent_state;
  persistent_state.eeprom().reset();
  persistent_state.eeprom().configure_factory_settings({sim::MachineModel::CarveraC1, 0x04});
  sim::MachineSimulator simulator(persistent_state);

  Kernel kernel;
  kernel.config = new Config(new MemoryConfigSource({
      "main_button_pin 1.16^\n",
      "main_button_LED_R_pin 1.10\n",
      "main_button_LED_G_pin 1.15\n",
      "main_button_LED_B_pin 1.14\n",
      "main_button_poll_frequency 20\n",
      "main_button_long_press_time 3000\n",
      "main_button_long_press_enable None\n",
      "e_stop_pin 0.26!^\n",
      "ps12_pin 0.22\n",
      "ps24_pin 0.10\n",
      "power_fan_delay_s 30\n",
      "power.auto_sleep false\n",
      "power.auto_sleep_min 5\n",
      "switch.light.startup_state false\n",
      "light.turn_off_min 0\n",
      "stop_on_cover_open false\n",
      "sd_ok true\n",
  }));
  kernel.config->config_cache_load();

  simulator.set_e_stop_pressed(sim::MachineModel::CarveraC1, false);
  kernel.add_module(new MainButton());

  auto rails = simulator.power_rails();
  require(rails.v12, "MainButton should turn on the 12V rail at module load");
  require(rails.v24, "MainButton should turn on the 24V rail at module load");

  char e_stop_state = 1;
  require(PublicData::get_value(main_button_checksum, get_e_stop_state_checksum, 0, &e_stop_state),
          "MainButton should publish e-stop state through PublicData");
  require(e_stop_state == 0, "C1 e-stop should start unpressed when the inverted input is physically high");

  simulator.set_e_stop_pressed(sim::MachineModel::CarveraC1, true);
  kernel.call_event(ON_IDLE, nullptr);
  require(kernel.is_halted(), "MainButton should halt the firmware when the e-stop input is pressed");
  require(kernel.get_halt_reason() == E_STOP, "MainButton should report E_STOP as the halt reason");

  char power_off = 0;
  require(PublicData::set_value(main_button_checksum, switch_power_12_checksum, &power_off),
          "MainButton should accept 12V rail PublicData writes");
  rails = simulator.power_rails();
  require(!rails.v12, "12V rail PublicData write should drive the power GPIO low");
  require(rails.v24, "12V rail PublicData write should leave the 24V rail alone");

  return 0;
}
