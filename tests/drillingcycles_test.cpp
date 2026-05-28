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
#include "libs/Kernel.h"
#include "modules/tools/drillingcycles/Drillingcycles.h"
#include "sim/machine_simulator.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "drillingcycles.enable true\n",
      "drillingcycles.dwell_units P\n",
  }));
  kernel.config->config_cache_load();

  Drillingcycles drillingcycles;
  drillingcycles.on_module_loaded();

  require(kernel.kernel_has_event(ON_GCODE_RECEIVED, &drillingcycles),
          "enabled Drillingcycles should register for G-code events");

  return 0;
}
