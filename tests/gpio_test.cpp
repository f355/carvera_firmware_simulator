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

#include <cstdint>
#include <iostream>

#include "gpio.h"
#include "lpc1768_sim.h"
#include "lpc17xx_pinsel.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::lpc1768::reset();

  GPIO pin(P1_18);

  pin.output();
  pin.set();

  require((sim::lpc1768::gpio_port(1).FIODIR & (1u << 18)) != 0, "P1.18 should be configured as output");
  require((sim::lpc1768::gpio_port(1).FIOPIN & (1u << 18)) != 0, "P1.18 should read high after set()");
  require(pin.get() == 255, "GPIO::get() should return 255 for a high pin");

  pin.clear();

  require((sim::lpc1768::gpio_port(1).FIOPIN & (1u << 18)) == 0, "P1.18 should read low after clear()");
  require(pin.get() == 0, "GPIO::get() should return 0 for a low pin");

  GPIO encoded(P2_10);
  require(encoded.port == 2, "PinName encoding should preserve the port");
  require(encoded.pin == 10, "PinName encoding should preserve the pin");

  PINSEL_CFG_Type pin_config{};
  pin_config.Portnum = 1;
  pin_config.Pinnum = 18;
  pin_config.Funcnum = 2;
  pin_config.Pinmode = PINSEL_PINMODE_PULLDOWN;
  pin_config.OpenDrain = PINSEL_PINMODE_OPENDRAIN;
  PINSEL_ConfigPin(&pin_config);

  require(((LPC_PINCON->PINSEL3 >> 4) & 0x3u) == 2, "PINSEL_ConfigPin should set P1.18 function bits");
  require(((LPC_PINCON->PINMODE3 >> 4) & 0x3u) == PINSEL_PINMODE_PULLDOWN,
          "PINSEL_ConfigPin should set P1.18 resistor mode bits");
  require((LPC_PINCON->PINMODE_OD1 & (1u << 18)) != 0, "PINSEL_ConfigPin should enable P1.18 open-drain mode");

  pin_config.Funcnum = 0;
  pin_config.Pinmode = PINSEL_PINMODE_TRISTATE;
  pin_config.OpenDrain = PINSEL_PINMODE_NORMAL;
  PINSEL_ConfigPin(&pin_config);

  require(((LPC_PINCON->PINSEL3 >> 4) & 0x3u) == 0, "PINSEL_ConfigPin should replace function bits");
  require(((LPC_PINCON->PINMODE3 >> 4) & 0x3u) == PINSEL_PINMODE_TRISTATE,
          "PINSEL_ConfigPin should replace resistor mode bits");
  require((LPC_PINCON->PINMODE_OD1 & (1u << 18)) == 0, "PINSEL_ConfigPin should disable P1.18 open-drain mode");

  return 0;
}
