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

#ifndef SIMULATOR_SERIAL_H
#define SIMULATOR_SERIAL_H

#include <cstddef>
#include <deque>
#include <functional>
#include <string>

#include "PinNames.h"

namespace mbed {

class Serial {
 public:
  enum Parity { None = 0, Odd, Even, Forced1, Forced0 };

  enum IrqType { RxIrq = 0, TxIrq };

  Serial(PinName tx, PinName rx, const char* name = nullptr)
      : tx_pin_(tx), rx_pin_(rx), name_(name == nullptr ? "" : name) {}

  void baud(int baudrate) { baudrate_ = baudrate; }
  int baudrate() const { return baudrate_; }

  void format(int bits = 8, Parity parity = Serial::None, int stop_bits = 1) {
    bits_ = bits;
    parity_ = parity;
    stop_bits_ = stop_bits;
  }

  int readable() const { return rx_.empty() ? 0 : 1; }
  int writeable() const { return 1; }

  void attach(void (*fptr)(void), IrqType type = RxIrq) {
    irq_[type] = fptr == nullptr ? std::function<void()>{} : std::function<void()>{fptr};
  }

  template <typename T>
  void attach(T* tptr, void (T::*mptr)(void), IrqType type = RxIrq) {
    if (tptr == nullptr || mptr == nullptr) {
      irq_[type] = {};
      return;
    }
    irq_[type] = [tptr, mptr]() { (tptr->*mptr)(); };
  }

  int getc() {
    if (rx_.empty()) {
      return 0;
    }
    const auto c = static_cast<unsigned char>(rx_.front());
    rx_.pop_front();
    return c;
  }

  int putc(int c) {
    tx_.push_back(static_cast<char>(c));
    if (irq_[TxIrq]) {
      irq_[TxIrq]();
    }
    return c;
  }

  void simulate_rx(const std::string& bytes) {
    for (char c : bytes) {
      wire_.push_back(c);
    }
    service_rx_irq();
  }

  // Hand the firmware at most one hardware FIFO per interrupt, the way a real
  // UART does: the rest of the burst is still in flight and arrives on later
  // interrupts. Firmware that parses whole commands out of the FIFO therefore
  // yields to the main loop between them instead of consuming an unbounded
  // burst in a single ISR call.
  void service_rx_irq() {
    while (!wire_.empty() && rx_.size() < kRxFifoBytes) {
      rx_.push_back(wire_.front());
      wire_.pop_front();
    }
    if (!rx_.empty() && irq_[RxIrq]) {
      irq_[RxIrq]();
    }
  }

  bool rx_in_flight() const { return !wire_.empty() || !rx_.empty(); }

  std::string take_tx() {
    auto bytes = tx_;
    tx_.clear();
    return bytes;
  }

  PinName tx_pin() const { return tx_pin_; }
  PinName rx_pin() const { return rx_pin_; }
  const std::string& name() const { return name_; }

 private:
  PinName tx_pin_;
  PinName rx_pin_;
  std::string name_;
  int baudrate_{9600};
  int bits_{8};
  Parity parity_{None};
  int stop_bits_{1};
  // LPC1768 UART RX FIFO depth.
  static constexpr std::size_t kRxFifoBytes = 16;

  std::deque<char> wire_;  // bytes still arriving, not yet in the FIFO
  std::deque<char> rx_;    // bytes the firmware can read now
  std::string tx_;
  std::function<void()> irq_[2];
};

}  // namespace mbed

#endif
