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

#ifndef SIMULATOR_SIM_MACHINE_SIMULATOR_HPP
#define SIMULATOR_SIM_MACHINE_SIMULATOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

#include "sim/i2c_eeprom.hpp"
#include "sim/lpc1768.hpp"
#include "sim/pin_address.hpp"
#include "sim/stepper_axis.hpp"

namespace sim {

struct PwmOutputState {
  bool configured{false};
  float duty{0.0F};
  float period_us{0.0F};
};

struct PowerRailState {
  bool v12{false};
  bool v24{false};
};

enum class TemperatureSensor {
  Spindle,
  Power,
};

class SimulatorContext;

class MachineSimulator {
 public:
  MachineSimulator();
  ~MachineSimulator();

  MachineSimulator(const MachineSimulator&) = delete;
  MachineSimulator& operator=(const MachineSimulator&) = delete;

  void reset(bool preserve_physical_scene = false);
  void advance_us(std::uint64_t delta_us);
  void start_realtime();
  void pause_realtime();
  bool is_realtime() const;
  bool set_realtime_speed(double speed);
  double realtime_speed() const;
  void poll();
  std::uint64_t time_us() const;

  SimulatorContext& context();
  const SimulatorContext& context() const;
  Lpc1768& mcu();

  void set_gpio_input(PinAddress pin, bool high);
  bool gpio_level(PinAddress pin) const;
  void set_main_button_pressed(MachineModel model, bool pressed);
  void set_e_stop_pressed(MachineModel model, bool pressed);
  PowerRailState power_rails() const;
  PwmOutputState pwm_output(PinAddress pin) const;
  void trigger_interrupt_rise(PinAddress pin);
  void set_adc_channel_raw(std::uint8_t channel, std::uint16_t raw12);
  std::uint16_t adc_channel_raw(std::uint8_t channel) const;
  void set_temperature(TemperatureSensor sensor, double celsius);
  void set_rotary_accessory_installed(bool installed);
  bool rotary_accessory_installed() const { return rotary_accessory_installed_; }

  std::size_t add_step_dir_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction = false);
  std::int64_t axis_position_steps(std::size_t axis) const;
  double axis_position_mm(std::size_t axis) const;
  bool axis_endstop_triggered(std::size_t axis) const;
  bool axis_endstop_triggered(std::size_t axis, EndstopSide side) const;

 private:
  std::unique_ptr<SimulatorContext> context_;
  SimulatorContext* previous_context_{nullptr};
  bool rotary_accessory_installed_{false};
};

}  // namespace sim

#endif
