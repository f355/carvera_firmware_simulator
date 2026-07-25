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

namespace {

using sim::test::require;

}  // namespace

int main() {
  sim::lpc_memory::set_ahb_capacity(256);
  require(!sim::lpc_memory::ahb_unlimited(), "capped AHB mode should not report unlimited");
  require(sim::lpc_memory::ahb_capacity() == 256, "AHB capacity should stick");

  void* a = simulator_ahb.alloc(64);
  require(a != nullptr, "first AHB allocation should succeed");
  void* b = simulator_ahb.alloc(64);
  require(b != nullptr, "second AHB allocation should succeed");
  require(simulator_ahb.alloc(200) == nullptr, "AHB pool should return nullptr when exhausted");

  simulator_ahb.dealloc(a);
  simulator_ahb.dealloc(b);
  void* c = simulator_ahb.alloc(128);
  require(c != nullptr, "coalesced free blocks should allow a larger allocation");
  simulator_ahb.dealloc(c);

  sim::lpc_memory::set_ahb_capacity(0);
  require(sim::lpc_memory::ahb_unlimited(), "capacity 0 should enable unlimited mode");
  void* big = simulator_ahb.alloc(1024 * 1024);
  require(big != nullptr, "unlimited AHB mode should malloc large blocks");
  simulator_ahb.dealloc(big);

  // Restore LPC-like default for later tests in this process.
  sim::lpc_memory::set_ahb_capacity(sim::lpc_memory::kDefaultAhbPoolBytes);
  return 0;
}
