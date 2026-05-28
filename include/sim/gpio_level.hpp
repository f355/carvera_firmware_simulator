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

#ifndef SIMULATOR_SIM_GPIO_LEVEL_HPP
#define SIMULATOR_SIM_GPIO_LEVEL_HPP

#include <cstdint>

#include "sim/lpc1768.hpp"
#include "sim/pin_address.hpp"

namespace sim::gpio {

inline void set_level(PinAddress pin, bool high) {
  auto& port = lpc1768::gpio_port(pin.port);
  const auto mask = static_cast<std::uint32_t>(1u << pin.pin);
  if (high) {
    port.FIOPIN |= mask;
  } else {
    port.FIOPIN &= ~mask;
  }
}

inline bool level(PinAddress pin) {
  const auto& port = lpc1768::active().gpio_port(pin.port);
  return (port.FIOPIN & (1u << pin.pin)) != 0;
}

}  // namespace sim::gpio

#endif
