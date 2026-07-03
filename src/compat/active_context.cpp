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

#include "compat/active_context.hpp"

#include <atomic>
#include <stdexcept>

#include "sim/simulator_context.hpp"

namespace sim::compat {
namespace {

std::atomic<SimulatorContext*> current_context{nullptr};

}  // namespace

SimulatorContext& active_context() {
  auto* context = current_context.load(std::memory_order_acquire);
  if (context == nullptr) {
    throw std::logic_error("simulator compatibility call requires an active MachineSimulator");
  }
  return *context;
}

SimulatorContext* try_active_context() noexcept { return current_context.load(std::memory_order_acquire); }

SimulatorContext* set_active_context(SimulatorContext* context) {
  return current_context.exchange(context, std::memory_order_acq_rel);
}

}  // namespace sim::compat
