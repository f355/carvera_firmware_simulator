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

#include "sim/us_ticker_sim.hpp"

#include "sim/simulator_context.hpp"
#include "sim/virtual_clock.hpp"
#include "compat/active_context.hpp"

namespace sim {

void UsTickerState::reset() {
  active_handler_ = nullptr;
  event_head_ = nullptr;
}

std::uint32_t UsTickerState::read() const { return static_cast<std::uint32_t>(clock::active().read_us()); }

void UsTickerState::set_handler(ticker_event_handler handler) { active_handler_ = handler; }

void UsTickerState::insert_event(ticker_event_t& event, unsigned int timestamp, std::uint32_t id) {
  event.timestamp = timestamp;
  event.id = id;
  event.next = nullptr;

  if (event_head_ == nullptr || static_cast<std::int32_t>(timestamp - event_head_->timestamp) < 0) {
    event.next = event_head_;
    event_head_ = &event;
    return;
  }

  auto* cursor = event_head_;
  while (cursor->next != nullptr && static_cast<std::int32_t>(timestamp - cursor->next->timestamp) >= 0) {
    cursor = cursor->next;
  }

  event.next = cursor->next;
  cursor->next = &event;
}

void UsTickerState::remove_event(ticker_event_t& event) {
  ticker_event_t** cursor = &event_head_;
  while (*cursor != nullptr) {
    if (*cursor == &event) {
      *cursor = event.next;
      event.next = nullptr;
      return;
    }
    cursor = &(*cursor)->next;
  }
}

void UsTickerState::dispatch_due_events() {
  while (active_handler_ != nullptr && event_head_ != nullptr && due(event_head_->timestamp)) {
    auto* event = event_head_;
    event_head_ = event_head_->next;
    event->next = nullptr;
    active_handler_(event->id);
  }
}

bool UsTickerState::due(std::uint32_t timestamp) const { return static_cast<std::int32_t>(timestamp - read()) <= 0; }

namespace us_ticker {

UsTickerState& active() { return compat::active_context().us_ticker(); }

void reset() { active().reset(); }

void dispatch_due_events() { active().dispatch_due_events(); }

}  // namespace us_ticker

}  // namespace sim

extern "C" uint32_t us_ticker_read(void) { return sim::us_ticker::active().read(); }

extern "C" void us_ticker_set_handler(ticker_event_handler handler) { sim::us_ticker::active().set_handler(handler); }

extern "C" void us_ticker_insert_event(ticker_event_t* obj, unsigned int timestamp, uint32_t id) {
  if (obj != nullptr) {
    sim::us_ticker::active().insert_event(*obj, timestamp, id);
  }
}

extern "C" void us_ticker_remove_event(ticker_event_t* obj) {
  if (obj != nullptr) {
    sim::us_ticker::active().remove_event(*obj);
  }
}
