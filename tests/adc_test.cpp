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

#include "Adc.h"
#include "LPC17xx.h"
#include "Pin.h"
#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

using sim::test::require;


int main() {
  sim::MachineSimulator simulator;
  Pin thermistor_pin;
  require(thermistor_pin.from_string("0.26") != nullptr, "ADC pin should parse");

  Adc adc;
  adc.enable_pin(&thermistor_pin);

  simulator.set_adc_channel_raw(3, 1024);
  unsigned int reading = 0;
  for (int i = 0; i < 4; ++i) {
    reading = adc.read(&thermistor_pin);
  }

  require(reading == 4096, "real Adc oversampling should expose simulator channel input");
  require((LPC_ADC->ADCR & (1u << 3)) != 0, "ADC channel 3 should be enabled in ADCR");
  require((LPC_ADC->ADINTEN & (1u << 3)) != 0, "ADC channel 3 interrupt should be enabled");
  require(simulator.adc_channel_raw(3) == 1024, "simulator should retain raw ADC channel value");

  return 0;
}
