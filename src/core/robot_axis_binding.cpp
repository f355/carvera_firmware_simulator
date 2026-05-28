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

#include "sim/robot_axis_binding.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "Config.h"
#include "ConfigValue.h"
#include "Pin.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "sim/board_profile.hpp"
#include "sim/gpio_level.hpp"
#include "sim/machine_geometry.hpp"
#include "sim/stepper_axis.hpp"

namespace {

#define MOTOR_CHECKSUMS(X) {CHECKSUM(X "_step_pin"), CHECKSUM(X "_dir_pin")}

constexpr uint16_t motor_checksums[][2] = {
    MOTOR_CHECKSUMS("alpha"), MOTOR_CHECKSUMS("beta"),    MOTOR_CHECKSUMS("gamma"),
    MOTOR_CHECKSUMS("delta"), MOTOR_CHECKSUMS("epsilon"),
};

constexpr uint16_t endstop_checksums[][3] = {
    {CHECKSUM("alpha_min_endstop"), CHECKSUM("alpha_max_endstop"), CHECKSUM("alpha_homing_direction")},
    {CHECKSUM("beta_min_endstop"), CHECKSUM("beta_max_endstop"), CHECKSUM("beta_homing_direction")},
    {CHECKSUM("gamma_min_endstop"), CHECKSUM("gamma_max_endstop"), CHECKSUM("gamma_homing_direction")},
    {CHECKSUM("delta_min_endstop"), CHECKSUM("delta_max_endstop"), CHECKSUM("delta_homing_direction")},
    {CHECKSUM("epsilon_min_endstop"), CHECKSUM("epsilon_max_endstop"), CHECKSUM("epsilon_homing_direction")},
};

constexpr uint16_t max_travel_checksums[] = {
    CHECKSUM("alpha_max_travel"), CHECKSUM("beta_max_travel"),    CHECKSUM("gamma_max_travel"),
    CHECKSUM("delta_max_travel"), CHECKSUM("epsilon_max_travel"),
};

constexpr uint16_t position_checksums[][2] = {
    {CHECKSUM("alpha_min"), CHECKSUM("alpha_max")},     {CHECKSUM("beta_min"), CHECKSUM("beta_max")},
    {CHECKSUM("gamma_min"), CHECKSUM("gamma_max")},     {CHECKSUM("delta_min"), CHECKSUM("delta_max")},
    {CHECKSUM("epsilon_min"), CHECKSUM("epsilon_max")},
};

constexpr uint16_t motor_alarm_checksums[] = {
    CHECKSUM("alpha_motor_alarm_pin"), CHECKSUM("beta_motor_alarm_pin"),    CHECKSUM("gamma_motor_alarm_pin"),
    CHECKSUM("delta_motor_alarm_pin"), CHECKSUM("epsilon_motor_alarm_pin"),
};

constexpr uint16_t atc_checksum = CHECKSUM("atc");
constexpr uint16_t atc_homing_endstop_pin_checksum = CHECKSUM("homing_endstop_pin");
constexpr double hard_limit_half_padding_mm = 1.0;
constexpr double rotary_initial_angle_degrees = 10.0;

sim::PinAddress pin_address(const Pin& pin) {
  return sim::PinAddress{static_cast<std::uint8_t>(pin.port_number), pin.pin};
}

bool same_pin(sim::PinAddress lhs, sim::PinAddress rhs) { return lhs.port == rhs.port && lhs.pin == rhs.pin; }

void set_pin_inactive(Pin& pin) {
  if (!pin.connected()) {
    return;
  }

  // Pin::get() returns inversion XOR raw level. External alarm inputs should
  // boot inactive until the simulator models the device that owns them.
  sim::gpio::set_level(pin_address(pin), pin.is_inverting());
}

std::pair<double, double> configured_travel_range(Kernel& kernel, std::size_t actuator, bool home_to_min) {
  const double home_position = home_to_min ? kernel.config->value(position_checksums[actuator][0])->as_number(0.0F)
                                           : kernel.config->value(position_checksums[actuator][1])->as_number(0.0F);
  const double max_travel = kernel.config->value(max_travel_checksums[actuator])->as_number(500.0F);
  return {
      home_to_min ? home_position : home_position - max_travel,
      home_to_min ? home_position + max_travel : home_position,
  };
}

}  // namespace

namespace sim {

void attach_configured_stepper_axes(Kernel& kernel) { attach_configured_stepper_axes(kernel, MachineModel::CarveraC1); }

void attach_configured_stepper_axes(Kernel& kernel, MachineModel model) {
  attach_configured_stepper_axes(kernel, model, true);
}

void attach_configured_stepper_axes(Kernel& kernel, MachineModel model, bool rotary_accessory_installed) {
  if (kernel.config == nullptr || kernel.robot == nullptr) {
    return;
  }

  const auto hardware_geometry = geometry_for(model);
  const auto motor_count = kernel.robot->get_number_registered_motors();
  for (std::size_t actuator = 0; actuator < motor_count && actuator < std::size(motor_checksums); ++actuator) {
    Pin step_pin;
    Pin direction_pin;
    step_pin.from_string(kernel.config->value(motor_checksums[actuator][0])->as_string("nc"));
    direction_pin.from_string(kernel.config->value(motor_checksums[actuator][1])->as_string("nc"));
    if (!step_pin.connected() || !direction_pin.connected()) {
      continue;
    }

    StepperAxisConfig axis_config;
    axis_config.step_pin = pin_address(step_pin);
    axis_config.direction_pin = pin_address(direction_pin);
    axis_config.invert_direction = direction_pin.is_inverting();
    axis_config.motor_connected = actuator != A_AXIS || rotary_accessory_installed;
    axis_config.steps_per_mm = kernel.robot->actuators[actuator]->get_steps_per_mm();
    axis_config.initial_position_mm =
        hardware_geometry.has_value() && actuator < hardware_geometry->axes.size()
            ? hardware_geometry->axes[actuator].initial_position_mm
            : (actuator == A_AXIS ? rotary_initial_angle_degrees : (actuator <= Z_AXIS ? -10.0 : 0.0));

    if (actuator < std::size(endstop_checksums)) {
      Pin min_endstop_pin;
      Pin max_endstop_pin;
      min_endstop_pin.from_string(kernel.config->value(endstop_checksums[actuator][0])->as_string("nc"));
      max_endstop_pin.from_string(kernel.config->value(endstop_checksums[actuator][1])->as_string("nc"));
      const bool rotary_shared_home_sensor = actuator > Z_AXIS && min_endstop_pin.connected() &&
                                             max_endstop_pin.connected() &&
                                             same_pin(pin_address(min_endstop_pin), pin_address(max_endstop_pin));
      const bool cartesian_shared_switch = actuator <= Z_AXIS && min_endstop_pin.connected() &&
                                           max_endstop_pin.connected() &&
                                           same_pin(pin_address(min_endstop_pin), pin_address(max_endstop_pin));
      const bool home_to_min =
          kernel.config->value(endstop_checksums[actuator][2])->as_string("home_to_min") != "home_to_max";
      double soft_min = 0.0;
      double soft_max = 0.0;
      if (actuator <= Z_AXIS) {
        soft_min = kernel.robot->get_soft_endstop_min(static_cast<int>(actuator));
        soft_max = kernel.robot->get_soft_endstop_max(static_cast<int>(actuator));
      }
      if (actuator > Z_AXIS || !std::isfinite(soft_min) || !std::isfinite(soft_max) || soft_min >= soft_max) {
        const auto range = configured_travel_range(kernel, actuator, home_to_min);
        soft_min = range.first;
        soft_max = range.second;
      }
      double min_switch_position = std::min(soft_min, soft_max) - hard_limit_half_padding_mm;
      double max_switch_position = std::max(soft_min, soft_max) + hard_limit_half_padding_mm;
      if (hardware_geometry.has_value() && actuator < hardware_geometry->axes.size() && !cartesian_shared_switch) {
        min_switch_position = hardware_geometry->axes[actuator].min_switch_mm;
        max_switch_position = hardware_geometry->axes[actuator].max_switch_mm;
      }

      const int home_side_index = home_to_min ? 0 : 1;

      for (int side_index = 0; side_index < 2; ++side_index) {
        if (rotary_shared_home_sensor && side_index != home_side_index) {
          continue;
        }
        if (actuator <= Z_AXIS) {
          const auto& profile_limits = board_profile(model).physical_limits[actuator];
          const auto& signal = side_index == 0 ? profile_limits.min : profile_limits.max;
          if (!signal.connected) {
            continue;
          }
        }
        Pin endstop_pin;
        endstop_pin.from_string(kernel.config->value(endstop_checksums[actuator][side_index])->as_string("nc"));
        if (!endstop_pin.connected()) {
          continue;
        }

        const bool is_min = side_index == 0;
        const double trigger_position = rotary_shared_home_sensor ? 0.0
                                        : is_min                  ? min_switch_position
                                                                  : max_switch_position;
        axis_config.endstops.push_back(StepperEndstopConfig{
            is_min ? EndstopSide::Min : EndstopSide::Max,
            true,
            pin_address(endstop_pin),
            trigger_position,
            !is_min,
            !endstop_pin.is_inverting(),
        });
      }
    }

    if (actuator < std::size(motor_alarm_checksums)) {
      Pin motor_alarm_pin;
      motor_alarm_pin.from_string(kernel.config->value(motor_alarm_checksums[actuator])->as_string("nc"));
      set_pin_inactive(motor_alarm_pin);
    }

    if (actuator == 4) {
      Pin atc_homing_pin;
      atc_homing_pin.from_string(
          kernel.config->value(atc_checksum, atc_homing_endstop_pin_checksum)->as_string("1.0^"));
      if (atc_homing_pin.connected()) {
        axis_config.endstops.push_back(StepperEndstopConfig{
            EndstopSide::Max,
            true,
            pin_address(atc_homing_pin),
            1.0,
            true,
            !atc_homing_pin.is_inverting(),
        });
      }
    }

    stepper_axes::add_axis(axis_config);
  }
}

}  // namespace sim
