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

#ifndef SIMULATOR_PWMOUT_H
#define SIMULATOR_PWMOUT_H

#include "PinNames.h"
#include "sim/mbed_peripheral_state.hpp"

namespace mbed {

using PwmOutState = sim::PwmOutPinState;

class PwmOut {
 public:
  using State = PwmOutState;

  explicit PwmOut(PinName pin) : pin_(pin) { mutable_state().configured = true; }

  void period(float seconds) { mutable_state().period_us = seconds * 1000000.0F; }
  void period_ms(float milliseconds) { mutable_state().period_us = milliseconds * 1000.0F; }
  void period_us(float microseconds) { mutable_state().period_us = microseconds; }
  void write(float duty) { mutable_state().duty = duty < 0.0F ? 0.0F : (duty > 1.0F ? 1.0F : duty); }
  float read() const { return state().duty; }

  PwmOut& operator=(float duty) {
    write(duty);
    return *this;
  }

  operator float() const { return read(); }

  PinName pin() const { return pin_; }
  float period_us() const { return state().period_us; }

  static State state(PinName pin) { return sim::mbed_peripherals::pwm_outputs().state(pin); }
  static void reset_states() { sim::mbed_peripherals::pwm_outputs().reset(); }

 private:
  State& mutable_state() { return sim::mbed_peripherals::pwm_outputs().mutable_state(pin_); }
  State state() const { return sim::mbed_peripherals::pwm_outputs().state(pin_); }

  PinName pin_;
};

}  // namespace mbed

#endif
