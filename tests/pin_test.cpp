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

#include "libs/Pin.h"
#include "lpc1768_sim.h"
#include "support/assertions.hpp"

namespace {

std::uint32_t two_bit_field(std::uint32_t value, std::uint8_t shift) { return (value >> shift) & 0x3u; }

}  // namespace

int main() {
  using sim::test::require;

  sim::lpc1768::reset();

  Pin pin;
  pin.from_string("1.18")->as_output();

  pin.set(true);
  require((sim::lpc1768::gpio_port(1).FIOPIN & (1u << 18)) != 0, "Pin::set(true) should raise FIOPIN through FIOSET");
  require(pin.get(), "Pin::get() should see the direct FIOSET write");

  pin.set(false);
  require((sim::lpc1768::gpio_port(1).FIOPIN & (1u << 18)) == 0, "Pin::set(false) should lower FIOPIN through FIOCLR");
  require(!pin.get(), "Pin::get() should see the direct FIOCLR write");

  Pin input;
  input.from_string("0.1")->as_input();
  input.set(true);
  require((sim::lpc1768::gpio_port(0).FIOPIN & (1u << 1)) == 0,
          "FIOSET on an input pin should not create an external high input level");
  require(!input.get(), "Pin::get() on an input should read the external input level, not the output latch");

  Pin probe;
  probe.from_string("2.6v")->as_input();
  require(probe.connected(), "probe-style pin syntax should parse as a connected pin");
  require(probe.port_number == 2 && probe.pin == 6, "probe-style pin syntax should preserve port and pin");
  require(!probe.is_inverting(), "probe-style pull-down syntax should not imply inversion");
  require(two_bit_field(LPC_PINCON->PINMODE4, 12) == 0x3u, "v modifier should configure PINCON pull-down mode");

  Pin inverted_pullup;
  inverted_pullup.from_string("1.16!^")->as_input();
  require(inverted_pullup.connected(), "inverted pull-up pin syntax should parse as a connected pin");
  require(inverted_pullup.is_inverting(), "! modifier should configure logical inversion");
  require(two_bit_field(LPC_PINCON->PINMODE3, 0) == 0x0u, "^ modifier should configure PINCON pull-up mode");

  return 0;
}
