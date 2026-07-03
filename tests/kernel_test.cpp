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

#include "libs/Kernel.h"

#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

class ProbeModule : public Module {
 public:
  void on_idle(void*) override { ++idle_calls; }
  int idle_calls{0};
};

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;
  ProbeModule probe;

  kernel.register_for_event(ON_IDLE, &probe);
  require(kernel.kernel_has_event(ON_IDLE, &probe), "Kernel should track registered event hooks");

  kernel.call_event(ON_IDLE);
  require(probe.idle_calls == 1, "Kernel should dispatch registered event hooks");

  kernel.unregister_for_event(ON_IDLE, &probe);
  require(!kernel.kernel_has_event(ON_IDLE, &probe), "Kernel should remove registered event hooks");

  kernel.call_event(ON_IDLE);
  require(probe.idle_calls == 1, "Kernel should not dispatch unregistered event hooks");

  return 0;
}
