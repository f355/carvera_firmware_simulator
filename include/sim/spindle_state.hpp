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

#ifndef SIMULATOR_SIM_SPINDLE_STATE_HPP
#define SIMULATOR_SIM_SPINDLE_STATE_HPP

#include <cstdint>
#include <mutex>

#include "sim/i2c_eeprom.hpp"

namespace sim {

namespace spindle_state {

constexpr double ramp_rate_rpm_per_second = 4000.0;

struct Snapshot {
  bool spinning{false};
  double actual_rpm{0.0};
  double target_rpm{0.0};
  double max_rpm{0.0};
};

double max_rpm(MachineModel model);
}  // namespace spindle_state

class SpindleState {
 public:
  void reset();
  void update(MachineModel model, bool enabled, double requested_rpm, std::uint64_t now_us);
  spindle_state::Snapshot snapshot(MachineModel model) const;
  double actual_rpm() const;
  double target_rpm() const;

 private:
  struct State {
    double actual_rpm{0.0};
    double target_rpm{0.0};
    std::uint64_t last_update_us{0};
    bool initialized{false};
  };

  mutable std::mutex mutex_{};
  State state_{};
};

}  // namespace sim

#endif
