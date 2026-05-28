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
#include "ZProbePublicAccess.h"
#include "libs/Kernel.h"
#include "modules/tools/zprobe/ZProbe.h"
#include "sim/machine_simulator.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "zprobe.enable true\n",
      "zprobe.probe_pin 2.6\n",
      "zprobe.calibrate_pin 0.5\n",
      "zprobe.debounce_ms 0\n",
  }));
  kernel.config->config_cache_load();

  auto* probe = new ZProbe();
  kernel.add_module(probe);

  char pin_states[2] = {};
  require(PublicData::get_value(zprobe_checksum, get_zprobe_pin_states_checksum, pin_states),
          "ZProbe should publish probe/tool-setter pin state through PublicData");
  require(pin_states[0] == 0, "probe pin should start low");
  require(pin_states[1] == 0, "tool-setter pin should start low");

  simulator.set_gpio_input({2, 6}, true);
  simulator.set_gpio_input({0, 5}, true);
  pin_states[0] = pin_states[1] = 0;
  require(PublicData::get_value(zprobe_checksum, get_zprobe_pin_states_checksum, pin_states),
          "ZProbe should keep publishing pin state after GPIO changes");
  require(pin_states[0] == 1, "probe pin should reflect physical GPIO input");
  require(pin_states[1] == 1, "tool-setter pin should reflect physical GPIO input");

  bool tlo_calibrating = true;
  require(PublicData::set_value(zprobe_checksum, set_tlo_calibrating_checksum, &tlo_calibrating),
          "ZProbe should accept TLO calibrating state through PublicData");

  return 0;
}
