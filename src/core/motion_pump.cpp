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

#include "sim/motion_pump.hpp"

#include "Robot.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/motion_telemetry.hpp"
#include "sim/physical_scene.hpp"
#include "sim/runtime_motor_alarm_wiring.hpp"
#include "sim/stepper_axis.hpp"
#include "sim/timer_irq.hpp"

namespace sim {
namespace {

void service_physical_model(Kernel& kernel) {
  if (stepper_axes::count() >= 3) {
    const Point3 spindle_position{
        stepper_axes::position_mm(0),
        stepper_axes::position_mm(1),
        stepper_axes::position_mm(2),
    };
    physical_scene::active().update_probe_contacts(spindle_position);
    if (stepper_axes::count() > 4) {
      physical_scene::active().update_atc_clamp_position(spindle_position, stepper_axes::position_mm(4));
    }
  }
  if (const auto crash_axis = physical_scene::active().stock_probe_crash_axis(); crash_axis.has_value()) {
    runtime_motor_alarm_wiring::drive(*crash_axis, true);
  }
  motion_telemetry::active().observe(kernel);
}

}  // namespace

void pump_motion(Kernel& kernel, std::size_t iterations) {
  for (std::size_t i = 0; i < iterations; ++i) {
    const bool advanced = timer_irq::advance_to_next_match();

    service_physical_model(kernel);

    if (!advanced) {
      break;
    }
  }
}

}  // namespace sim
