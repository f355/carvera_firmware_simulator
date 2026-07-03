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

#include "sim/mbed_peripheral_state.hpp"

#include <utility>

#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace sim {

void PwmOutRegistry::reset() { states_.fill(State{}); }

PwmOutRegistry::State PwmOutRegistry::state(PinName pin) const { return states_[index(pin)]; }

PwmOutRegistry::State& PwmOutRegistry::mutable_state(PinName pin) { return states_[index(pin)]; }

void InterruptInRegistry::reset() { handlers_ = {}; }

void InterruptInRegistry::set_rise_handler(PinName pin, std::function<void()> handler) {
  handlers_[index(pin)] = std::move(handler);
}

void InterruptInRegistry::simulate_rise(PinName pin) {
  auto& handler = handlers_[index(pin)];
  if (handler) {
    handler();
  }
}

namespace mbed_peripherals {

PwmOutRegistry& pwm_outputs() { return compat::active_context().pwm_outputs(); }

InterruptInRegistry& interrupts() { return compat::active_context().interrupts(); }

}  // namespace mbed_peripherals

}  // namespace sim
