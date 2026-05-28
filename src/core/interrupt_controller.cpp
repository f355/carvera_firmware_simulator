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

#include "sim/interrupt_controller.hpp"

#include <algorithm>
#include <utility>

#include "sim/lpc1768.hpp"
#include "sim/simulator_context.hpp"

namespace sim {

void InterruptController::reset() {
  pending_.clear();
  next_sequence_ = 0;
}

void InterruptController::raise(IRQn_Type irq, Handler handler) {
  pending_.push_back(PendingInterrupt{irq, next_sequence_++, std::move(handler)});
}

bool InterruptController::dispatch_next() {
  auto selected = highest_priority_pending();
  if (selected == pending_.end()) {
    return false;
  }

  auto handler = std::move(selected->handler);
  pending_.erase(selected);
  if (handler) {
    handler();
  }
  return true;
}

void InterruptController::dispatch_all() {
  while (dispatch_next()) {
  }
}

bool InterruptController::eligible(const PendingInterrupt& pending) const {
  return lpc1768::irq_globally_enabled() && lpc1768::irq_enabled(pending.irq);
}

std::vector<InterruptController::PendingInterrupt>::iterator InterruptController::highest_priority_pending() {
  auto selected = pending_.end();
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (!eligible(*it)) {
      continue;
    }
    if (selected == pending_.end()) {
      selected = it;
      continue;
    }

    const auto lhs_priority = lpc1768::nvic().priority(it->irq);
    const auto rhs_priority = lpc1768::nvic().priority(selected->irq);
    if (lhs_priority < rhs_priority || (lhs_priority == rhs_priority && it->sequence < selected->sequence)) {
      selected = it;
    }
  }
  return selected;
}

namespace interrupt_controller {

InterruptController& active() { return simulator_context::active().interrupt_controller(); }

void reset() { active().reset(); }

}  // namespace interrupt_controller

}  // namespace sim
