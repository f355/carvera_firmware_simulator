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

#ifndef SIMULATOR_SIM_MBED_PERIPHERAL_STATE_HPP
#define SIMULATOR_SIM_MBED_PERIPHERAL_STATE_HPP

#include <array>
#include <cstddef>
#include <functional>

#include "PinNames.h"

namespace sim {

struct PwmOutPinState {
  float duty{0.0F};
  float period_us{0.0F};
  bool configured{false};
};

class PwmOutRegistry {
 public:
  using State = PwmOutPinState;

  void reset();
  State state(PinName pin) const;
  State& mutable_state(PinName pin);

 private:
  static std::size_t index(PinName pin) { return static_cast<std::size_t>(pin); }

  std::array<State, 160> states_{};
};

class InterruptInRegistry {
 public:
  void reset();
  void set_rise_handler(PinName pin, std::function<void()> handler);
  void simulate_rise(PinName pin);

 private:
  static std::size_t index(PinName pin) { return static_cast<std::size_t>(pin); }

  std::array<std::function<void()>, 160> handlers_{};
};

namespace mbed_peripherals {

PwmOutRegistry& pwm_outputs();
InterruptInRegistry& interrupts();

}  // namespace mbed_peripherals

}  // namespace sim

#endif
