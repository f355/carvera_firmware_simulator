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

#ifndef SIMULATOR_SIM_TIMER_SCHEDULER_HPP
#define SIMULATOR_SIM_TIMER_SCHEDULER_HPP

#include <cstdint>
#include <optional>

namespace sim {

class TimerScheduler {
 public:
  void reset();
  bool dispatch_match(std::uint8_t timer_index);
  void advance_cycles(std::uint32_t cycles);
  bool advance_to_next_match();

 private:
  static constexpr std::uint8_t timer_count = 3;

  bool match_interrupt_enabled(std::uint8_t timer_index) const;
  std::optional<std::uint32_t> cycles_to_next_match() const;
  void advance_running_timers(std::uint32_t cycles);
  bool raise_due_matches();
  bool raise_match(std::uint8_t timer_index);

  std::uint64_t clock_cycle_remainder_{0};
};

namespace timer_scheduler {
TimerScheduler& active();
void reset();
}  // namespace timer_scheduler

}  // namespace sim

#endif
