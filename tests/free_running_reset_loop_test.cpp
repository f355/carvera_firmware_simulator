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

#include "libs/Kernel.h"
#include "sim/lpc_memory_constraints.hpp"
#include "sim/system_reset.hpp"
#include "support/booted_runtime.hpp"

namespace {

using sim::test::require;

}  // namespace

int main() {
  sim::test::BootedRuntime runtime;
  require(runtime.kernel().conveyor != nullptr, "booted firmware should construct the conveyor");

  const auto before = sim::lpc_memory::firmware_reboot_count();
  sim::system_reset::request();

  bool saw_reset = false;
  for (int i = 0; i < 16; ++i) {
    const auto result = runtime.runtime().runner().pump_free_running_result(4, 1000);
    if (result.reset_requested) {
      saw_reset = true;
      break;
    }
  }
  require(saw_reset, "free-running pump should observe system_reset");
  require(sim::lpc_memory::firmware_reboot_count() == before + 1,
          "system_reset should count as an LPC-like firmware reboot");

  // Next free-running pump reconstructs Kernel (boot loop step).
  (void)runtime.runtime().runner().pump_free_running_result(0, 0);
  require(runtime.runtime().booted(), "free-running mode should re-boot after system_reset");
  require(runtime.runtime().boot().conveyor != nullptr,
          "rebooted firmware should reconstruct core modules");
  return 0;
}
