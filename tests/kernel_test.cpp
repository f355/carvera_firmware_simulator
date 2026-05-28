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

#include <cstdlib>
#include <iostream>

#include "libs/Kernel.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

class ProbeModule : public Module {
 public:
  void on_idle(void*) override { ++idle_calls; }
  int idle_calls{0};
};

}  // namespace

int main() {
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
