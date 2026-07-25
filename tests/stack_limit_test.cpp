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

#include "sim/lpc_memory_constraints.hpp"
#include "sim/system_reset.hpp"

namespace {

using sim::test::require;

void deep(int depth) {
  volatile char pad[64];
  pad[0] = static_cast<char>(depth);
  sim::lpc_memory::check_firmware_stack_sample();
  if (depth > 0) {
    deep(depth - 1);
  }
  (void)pad[0];
}

}  // namespace

int main() {
  // Tiny native budget so a shallow recursive walk trips the optional check.
  sim::lpc_memory::set_native_stack_limit(256);
  require(sim::system_reset::consume_requested() == false, "reset flag should start clear");

  sim::lpc_memory::begin_firmware_stack_sample();
  deep(64);
  require(sim::system_reset::consume_requested(),
          "optional native stack-limit check should request system_reset when exceeded");

  sim::lpc_memory::set_native_stack_limit(0);
  return 0;
}
