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

#include "sim/stepper_axis.hpp"

#include <cmath>
#include <stdexcept>

#include "sim/gpio_level.hpp"
#include "sim/lpc1768.hpp"
#include "sim/simulator_context.hpp"

namespace sim {

StepperAxis::StepperAxis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction)
    : StepperAxis(StepperAxisConfig{step_pin, direction_pin, invert_direction, true, 1.0, 0.0, {}}) {}

StepperAxis::StepperAxis(const StepperAxisConfig& config)
    : step_pin_(config.step_pin),
      direction_pin_(config.direction_pin),
      endstops_(config.endstops),
      invert_direction_(config.invert_direction),
      motor_connected_(config.motor_connected),
      steps_per_mm_(config.steps_per_mm > 0.0 ? config.steps_per_mm : 1.0),
      initial_position_steps_(static_cast<std::int64_t>(std::llround(config.initial_position_mm * steps_per_mm_))),
      position_steps_(initial_position_steps_) {
  step_high_ = gpio::level(step_pin_);
  direction_high_ = gpio::level(direction_pin_);
  update_endstop_pin();
}

void StepperAxis::reset_position() {
  position_steps_ = initial_position_steps_;
  step_high_ = false;
  direction_high_ = false;
  update_endstop_pin();
}

void StepperAxis::on_gpio_level_changed(PinAddress pin, bool high) {
  if (same_pin(pin, direction_pin_)) {
    direction_high_ = high;
    return;
  }

  if (!same_pin(pin, step_pin_)) {
    return;
  }

  const bool rising_edge = high && !step_high_;
  step_high_ = high;
  if (!rising_edge || !motor_connected_) {
    return;
  }

  position_steps_ += (direction_high_ ^ invert_direction_) ? -1 : 1;
  update_endstop_pin();
}

double StepperAxis::position_mm() const { return static_cast<double>(position_steps_) / steps_per_mm_; }

bool StepperAxis::endstop_triggered() const {
  for (const auto& endstop : endstops_) {
    if (endstop_triggered(endstop)) {
      return true;
    }
  }

  return false;
}

bool StepperAxis::endstop_triggered(EndstopSide side) const {
  for (const auto& endstop : endstops_) {
    if (endstop.side == side && endstop_triggered(endstop)) {
      return true;
    }
  }

  return false;
}

void StepperAxis::set_motor_connected(bool connected) {
  motor_connected_ = connected;
  update_endstop_pin();
}

bool StepperAxis::endstop_triggered(const StepperEndstopConfig& endstop) const {
  if (!endstop.enabled) {
    return false;
  }

  if (!motor_connected_) {
    return true;
  }

  const double position = position_mm();
  if (endstop.trigger_when_greater_or_equal) {
    return position >= endstop.trigger_position_mm;
  }

  return position <= endstop.trigger_position_mm;
}

bool StepperAxis::same_pin(PinAddress lhs, PinAddress rhs) { return lhs.port == rhs.port && lhs.pin == rhs.pin; }

void StepperAxis::update_endstop_pin() {
  for (const auto& pin_owner : endstops_) {
    if (!pin_owner.enabled) {
      continue;
    }

    bool already_handled = false;
    for (const auto& previous : endstops_) {
      if (&previous == &pin_owner) {
        break;
      }
      if (previous.enabled && same_pin(previous.pin, pin_owner.pin)) {
        already_handled = true;
        break;
      }
    }
    if (already_handled) {
      continue;
    }

    bool any_triggered = false;
    bool raw_level_when_triggered = pin_owner.raw_level_when_triggered;
    for (const auto& endstop : endstops_) {
      if (!endstop.enabled || !same_pin(endstop.pin, pin_owner.pin)) {
        continue;
      }
      raw_level_when_triggered = endstop.raw_level_when_triggered;
      any_triggered = any_triggered || endstop_triggered(endstop);
    }

    auto& port = lpc1768::gpio_port(pin_owner.pin.port);
    const auto mask = static_cast<std::uint32_t>(1u << pin_owner.pin.pin);
    const bool raw_high = any_triggered ? raw_level_when_triggered : !raw_level_when_triggered;
    if (raw_high) {
      port.FIOPIN |= mask;
    } else {
      port.FIOPIN &= ~mask;
    }
  }
}

void StepperAxisRegistry::clear() { axes_.clear(); }

std::size_t StepperAxisRegistry::add_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction) {
  axes_.emplace_back(step_pin, direction_pin, invert_direction);
  return axes_.size() - 1;
}

std::size_t StepperAxisRegistry::add_axis(const StepperAxisConfig& config) {
  axes_.emplace_back(config);
  return axes_.size() - 1;
}

std::int64_t StepperAxisRegistry::position_steps(std::size_t axis) const {
  if (axis >= axes_.size()) {
    throw std::out_of_range("stepper axis index");
  }
  return axes_[axis].position_steps();
}

double StepperAxisRegistry::position_mm(std::size_t axis) const {
  if (axis >= axes_.size()) {
    throw std::out_of_range("stepper axis index");
  }
  return axes_[axis].position_mm();
}

bool StepperAxisRegistry::endstop_triggered(std::size_t axis) const {
  if (axis >= axes_.size()) {
    throw std::out_of_range("stepper axis index");
  }
  return axes_[axis].endstop_triggered();
}

bool StepperAxisRegistry::endstop_triggered(std::size_t axis, EndstopSide side) const {
  if (axis >= axes_.size()) {
    throw std::out_of_range("stepper axis index");
  }
  return axes_[axis].endstop_triggered(side);
}

void StepperAxisRegistry::set_motor_connected(std::size_t axis, bool connected) {
  if (axis >= axes_.size()) {
    throw std::out_of_range("stepper axis index");
  }
  axes_[axis].set_motor_connected(connected);
}

void StepperAxisRegistry::on_gpio_level_changed(PinAddress pin, bool high) {
  for (auto& axis : axes_) {
    axis.on_gpio_level_changed(pin, high);
  }
}

namespace stepper_axes {

StepperAxisRegistry& active() { return simulator_context::active().stepper_axes(); }

void clear() { active().clear(); }

std::size_t add_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction) {
  return active().add_axis(step_pin, direction_pin, invert_direction);
}

std::size_t add_axis(const StepperAxisConfig& config) { return active().add_axis(config); }

std::size_t count() { return active().count(); }

std::int64_t position_steps(std::size_t axis) { return active().position_steps(axis); }

double position_mm(std::size_t axis) { return active().position_mm(axis); }

bool endstop_triggered(std::size_t axis) { return active().endstop_triggered(axis); }

bool endstop_triggered(std::size_t axis, EndstopSide side) { return active().endstop_triggered(axis, side); }

void set_motor_connected(std::size_t axis, bool connected) { active().set_motor_connected(axis, connected); }

void on_gpio_level_changed(PinAddress pin, bool high) { active().on_gpio_level_changed(pin, high); }

}  // namespace stepper_axes

}  // namespace sim
