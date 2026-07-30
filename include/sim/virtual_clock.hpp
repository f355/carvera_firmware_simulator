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

#ifndef SIMULATOR_SIM_VIRTUAL_CLOCK_HPP
#define SIMULATOR_SIM_VIRTUAL_CLOCK_HPP

#include <cstdint>

namespace sim {

class VirtualClock {
 public:
  enum class Mode {
    Manual,
    Realtime,
  };

  void reset();
  void advance_us(std::uint64_t delta_us);
  void start_realtime();
  void pause_realtime();
  bool is_realtime() const { return mode_ == Mode::Realtime; }
  bool set_realtime_speed(double speed);
  double realtime_speed() const { return realtime_speed_; }
  std::uint64_t read_us() const;

 private:
  Mode mode_{Mode::Manual};
  std::uint64_t base_us_{0};
  double realtime_speed_{1.0};
};

namespace clock {
VirtualClock& active();
void reset();
}  // namespace clock

}  // namespace sim

#endif
