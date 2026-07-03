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

#include "sim/simulator_context.hpp"

namespace sim {

void SimulatorContext::reset(bool preserve_physical_scene) {
  clock_.reset();
  us_ticker_.reset();
  mcu_.reset();
  pwm_outputs_.reset();
  interrupts_.reset();
  main_button_led_.reset();
  spindle_.reset();
  stepper_axes_.clear();
  motion_telemetry_.reset();
  m8266_wifi_.reset();
  motor_alarm_wiring_.clear();
  persistent_state_.eeprom().reset_transaction();
  interrupt_controller_.reset();
  realtime_timer_pacer_.reset();
  timer_scheduler_.reset();
  adc_.reset();
  if (!preserve_physical_scene) {
    physical_scene_.clear();
  }
}

}  // namespace sim
