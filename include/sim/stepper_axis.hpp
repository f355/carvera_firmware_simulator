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

#ifndef SIMULATOR_SIM_STEPPER_AXIS_HPP
#define SIMULATOR_SIM_STEPPER_AXIS_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sim/pin_address.hpp"

namespace sim {

enum class EndstopSide {
  Min,
  Max,
};

struct StepperEndstopConfig {
  EndstopSide side{EndstopSide::Max};
  bool enabled{false};
  PinAddress pin{};
  double trigger_position_mm{0.0};
  bool trigger_when_greater_or_equal{true};
  bool raw_level_when_triggered{true};
};

struct StepperAxisConfig {
  PinAddress step_pin{};
  PinAddress direction_pin{};
  bool invert_direction{false};
  bool motor_connected{true};
  double steps_per_mm{1.0};
  double initial_position_mm{0.0};
  std::vector<StepperEndstopConfig> endstops{};
};

class StepperAxis {
 public:
  StepperAxis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction = false);
  explicit StepperAxis(const StepperAxisConfig& config);

  void reset_position();
  void on_gpio_level_changed(PinAddress pin, bool high);

  std::int64_t position_steps() const { return position_steps_; }
  double position_mm() const;
  bool endstop_triggered() const;
  bool endstop_triggered(EndstopSide side) const;
  void set_motor_connected(bool connected);

 private:
  bool endstop_triggered(const StepperEndstopConfig& endstop) const;
  void update_endstop_pin();

  PinAddress step_pin_{};
  PinAddress direction_pin_{};
  std::vector<StepperEndstopConfig> endstops_{};
  bool invert_direction_{false};
  bool motor_connected_{true};
  bool step_high_{false};
  bool direction_high_{false};
  double steps_per_mm_{1.0};
  std::int64_t initial_position_steps_{0};
  std::int64_t position_steps_{0};
};

class StepperAxisRegistry {
 public:
  void clear();
  std::size_t add_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction = false);
  std::size_t add_axis(const StepperAxisConfig& config);
  std::size_t count() const { return axes_.size(); }
  std::int64_t position_steps(std::size_t axis) const;
  double position_mm(std::size_t axis) const;
  bool endstop_triggered(std::size_t axis) const;
  bool endstop_triggered(std::size_t axis, EndstopSide side) const;
  void set_motor_connected(std::size_t axis, bool connected);
  void on_gpio_level_changed(PinAddress pin, bool high);

 private:
  const StepperAxis& checked(std::size_t axis) const;
  StepperAxis& checked(std::size_t axis);

  std::vector<StepperAxis> axes_;
};

namespace stepper_axes {

StepperAxisRegistry& active();
void clear();
std::size_t add_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction = false);
std::size_t add_axis(const StepperAxisConfig& config);
std::size_t count();
std::int64_t position_steps(std::size_t axis);
double position_mm(std::size_t axis);
bool endstop_triggered(std::size_t axis);
bool endstop_triggered(std::size_t axis, EndstopSide side);
void set_motor_connected(std::size_t axis, bool connected);
void on_gpio_level_changed(PinAddress pin, bool high);

}  // namespace stepper_axes

}  // namespace sim

#endif
