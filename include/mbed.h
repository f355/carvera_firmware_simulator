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

#ifndef SIMULATOR_MBED_H
#define SIMULATOR_MBED_H

#include <cstdint>

#include "PinNames.h"
#include "Ticker.h"
#include "us_ticker_api.h"

namespace mbed {

class I2C {
 public:
  I2C(PinName, PinName);
  void frequency(int hz);
  void start();
  int write(int value);
  int read(int ack);
  void stop();

 private:
  int frequency_hz_{100000};
};

class SPI {
 public:
  SPI(PinName, PinName, PinName) {}
  void format(int) {}
  int write(int) { return 0; }
};

}  // namespace mbed

class Timeout {
 public:
  template <typename T>
  void attach(T*, void (T::*)(), float) {}

  void detach() {}
};

inline void wait(float) {}
inline void wait_us(int) {}

#endif
