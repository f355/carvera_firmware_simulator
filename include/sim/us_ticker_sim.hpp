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

#ifndef SIMULATOR_SIM_US_TICKER_SIM_HPP
#define SIMULATOR_SIM_US_TICKER_SIM_HPP

#include <cstdint>

#include "us_ticker_api.h"

namespace sim {

class UsTickerState {
 public:
  void reset();
  std::uint32_t read() const;
  void set_handler(ticker_event_handler handler);
  void insert_event(ticker_event_t& event, unsigned int timestamp, std::uint32_t id);
  void remove_event(ticker_event_t& event);
  void dispatch_due_events();

 private:
  bool due(std::uint32_t timestamp) const;

  ticker_event_handler active_handler_{nullptr};
  ticker_event_t* event_head_{nullptr};
};

namespace us_ticker {

UsTickerState& active();
void reset();
void dispatch_due_events();

}  // namespace us_ticker

}  // namespace sim

#endif
