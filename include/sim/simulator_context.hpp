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

#ifndef SIMULATOR_SIM_SIMULATOR_CONTEXT_HPP
#define SIMULATOR_SIM_SIMULATOR_CONTEXT_HPP

#include "sim/adc.hpp"
#include "sim/interrupt_controller.hpp"
#include "sim/lpc1768.hpp"
#include "sim/m8266_wifi.hpp"
#include "sim/main_button_led.hpp"
#include "sim/mbed_peripheral_state.hpp"
#include "sim/motion_telemetry.hpp"
#include "sim/physical_scene.hpp"
#include "sim/persistent_machine_state.hpp"
#include "sim/realtime_timer_pacer.hpp"
#include "sim/runtime_motor_alarm_wiring.hpp"
#include "sim/spindle_state.hpp"
#include "sim/stepper_axis.hpp"
#include "sim/timer_scheduler.hpp"
#include "sim/us_ticker_sim.hpp"
#include "sim/virtual_clock.hpp"

namespace sim {

class SimulatorContext {
 public:
  explicit SimulatorContext(PersistentMachineState& persistent_state) : persistent_state_(persistent_state) {}

  void reset(bool preserve_physical_scene = false);

  VirtualClock& clock() { return clock_; }
  const VirtualClock& clock() const { return clock_; }

  Lpc1768& mcu() { return mcu_; }
  const Lpc1768& mcu() const { return mcu_; }

  AdcState& adc() { return adc_; }
  const AdcState& adc() const { return adc_; }

  PwmOutRegistry& pwm_outputs() { return pwm_outputs_; }
  const PwmOutRegistry& pwm_outputs() const { return pwm_outputs_; }

  InterruptInRegistry& interrupts() { return interrupts_; }
  const InterruptInRegistry& interrupts() const { return interrupts_; }

  StepperAxisRegistry& stepper_axes() { return stepper_axes_; }
  const StepperAxisRegistry& stepper_axes() const { return stepper_axes_; }

  InterruptController& interrupt_controller() { return interrupt_controller_; }
  const InterruptController& interrupt_controller() const { return interrupt_controller_; }

  RealtimeTimerPacer& realtime_timer_pacer() { return realtime_timer_pacer_; }
  const RealtimeTimerPacer& realtime_timer_pacer() const { return realtime_timer_pacer_; }

  TimerScheduler& timer_scheduler() { return timer_scheduler_; }
  const TimerScheduler& timer_scheduler() const { return timer_scheduler_; }

  UsTickerState& us_ticker() { return us_ticker_; }
  const UsTickerState& us_ticker() const { return us_ticker_; }

  MainButtonLedState& main_button_led() { return main_button_led_; }
  const MainButtonLedState& main_button_led() const { return main_button_led_; }

  SpindleState& spindle() { return spindle_; }
  const SpindleState& spindle() const { return spindle_; }

  MotionTelemetry& motion_telemetry() { return motion_telemetry_; }
  const MotionTelemetry& motion_telemetry() const { return motion_telemetry_; }

  PhysicalScene& physical_scene() { return physical_scene_; }
  const PhysicalScene& physical_scene() const { return physical_scene_; }

  m8266_wifi::Module& m8266_wifi() { return m8266_wifi_; }
  const m8266_wifi::Module& m8266_wifi() const { return m8266_wifi_; }

  runtime_motor_alarm_wiring::MotorAlarmWiring& motor_alarm_wiring() { return motor_alarm_wiring_; }
  const runtime_motor_alarm_wiring::MotorAlarmWiring& motor_alarm_wiring() const { return motor_alarm_wiring_; }

  I2cEepromDevice& eeprom() { return persistent_state_.eeprom(); }
  const I2cEepromDevice& eeprom() const { return persistent_state_.eeprom(); }

  HostFilesystem& host_filesystem() { return persistent_state_.host_filesystem(); }
  const HostFilesystem& host_filesystem() const { return persistent_state_.host_filesystem(); }

  PersistentMachineState& persistent_state() { return persistent_state_; }
  const PersistentMachineState& persistent_state() const { return persistent_state_; }

 private:
  VirtualClock clock_{};
  Lpc1768 mcu_{};
  AdcState adc_{};
  PwmOutRegistry pwm_outputs_{};
  InterruptInRegistry interrupts_{};
  StepperAxisRegistry stepper_axes_{};
  InterruptController interrupt_controller_{};
  RealtimeTimerPacer realtime_timer_pacer_{};
  TimerScheduler timer_scheduler_{};
  UsTickerState us_ticker_{};
  MainButtonLedState main_button_led_{};
  SpindleState spindle_{};
  MotionTelemetry motion_telemetry_{};
  PhysicalScene physical_scene_{};
  m8266_wifi::Module m8266_wifi_{};
  runtime_motor_alarm_wiring::MotorAlarmWiring motor_alarm_wiring_{};
  PersistentMachineState& persistent_state_;
};

}  // namespace sim

#endif
