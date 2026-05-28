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

#ifndef SIMULATOR_SIM_INTERRUPT_CONTROLLER_HPP
#define SIMULATOR_SIM_INTERRUPT_CONTROLLER_HPP

#include <cstdint>
#include <functional>
#include <vector>

#include "LPC17xx.h"

namespace sim {

class InterruptController {
 public:
  using Handler = std::function<void()>;

  void reset();
  void raise(IRQn_Type irq, Handler handler);
  bool dispatch_next();
  void dispatch_all();

 private:
  struct PendingInterrupt {
    IRQn_Type irq;
    std::uint64_t sequence;
    Handler handler;
  };

  bool eligible(const PendingInterrupt& pending) const;
  std::vector<PendingInterrupt>::iterator highest_priority_pending();

  std::vector<PendingInterrupt> pending_{};
  std::uint64_t next_sequence_{0};
};

namespace interrupt_controller {
InterruptController& active();
void reset();
}  // namespace interrupt_controller

}  // namespace sim

#endif
