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

#include "sim/machine_simulator.hpp"

#include <algorithm>
#include <cmath>

#include "InterruptIn.h"
#include "PwmOut.h"
#include "sim/adc.hpp"
#include "sim/board_profile.hpp"
#include "sim/gpio_level.hpp"
#include "sim/simulator_context.hpp"
#include "sim/us_ticker_sim.hpp"

namespace sim {

namespace {

std::uint16_t stock_beta_thermistor_raw(double celsius) {
  constexpr double r0 = 100000.0;
  constexpr double t0_kelvin = 25.0 + 273.15;
  constexpr double beta = 3950.0;
  constexpr double r2 = 4700.0;
  constexpr double adc_max = 4095.0;

  const double kelvin = celsius + 273.15;
  const double resistance = r0 * std::exp(beta * ((1.0 / kelvin) - (1.0 / t0_kelvin)));
  const double raw = adc_max * resistance / (resistance + r2);
  return static_cast<std::uint16_t>(std::clamp(std::lround(raw), 1L, 4094L));
}

std::uint8_t stock_temperature_channel(TemperatureSensor sensor) {
  switch (sensor) {
    case TemperatureSensor::Power:
      return 3;
    case TemperatureSensor::Spindle:
    default:
      return 5;
  }
}

}  // namespace

MachineSimulator::MachineSimulator()
    : context_(std::make_unique<SimulatorContext>()), previous_context_(simulator_context::activate(context_.get())) {
  context_->reset();
  context_->host_filesystem().copy_mounts_from(previous_context_->host_filesystem());
  context_->eeprom() = previous_context_->eeprom();
  set_temperature(TemperatureSensor::Spindle, 25.0);
  set_temperature(TemperatureSensor::Power, 25.0);
}

MachineSimulator::~MachineSimulator() { simulator_context::activate(previous_context_); }

void MachineSimulator::reset(bool preserve_physical_scene) {
  context_->reset(preserve_physical_scene);
  set_temperature(TemperatureSensor::Spindle, 25.0);
  set_temperature(TemperatureSensor::Power, 25.0);
  rotary_accessory_installed_ = false;
}

void MachineSimulator::advance_us(std::uint64_t delta_us) {
  context_->clock().advance_us(delta_us);
  poll();
}

void MachineSimulator::start_realtime() { context_->clock().start_realtime(); }

void MachineSimulator::pause_realtime() { context_->clock().pause_realtime(); }

bool MachineSimulator::is_realtime() const { return context_->clock().is_realtime(); }

bool MachineSimulator::set_realtime_speed(double speed) {
  if (!context_->clock().set_realtime_speed(speed)) {
    return false;
  }
  return true;
}

double MachineSimulator::realtime_speed() const { return context_->clock().realtime_speed(); }

void MachineSimulator::poll() { us_ticker::dispatch_due_events(); }

std::uint64_t MachineSimulator::time_us() const { return context_->clock().read_us(); }

SimulatorContext& MachineSimulator::context() { return *context_; }

const SimulatorContext& MachineSimulator::context() const { return *context_; }

Lpc1768& MachineSimulator::mcu() { return context_->mcu(); }

void MachineSimulator::set_gpio_input(PinAddress pin, bool high) { gpio::set_level(pin, high); }

bool MachineSimulator::gpio_level(PinAddress pin) const { return gpio::level(pin); }

void MachineSimulator::set_main_button_pressed(MachineModel model, bool pressed) {
  const auto& signal = board_profile(model).physical_main_button;
  set_gpio_input(signal.pin, signal_level(signal, pressed));
}

void MachineSimulator::set_e_stop_pressed(MachineModel model, bool pressed) {
  const auto& signal = board_profile(model).physical_e_stop;
  set_gpio_input(signal.pin, signal_level(signal, pressed));
}

PowerRailState MachineSimulator::power_rails() const {
  return PowerRailState{
      gpio_level({0, 22}),
      gpio_level({0, 10}),
  };
}

PwmOutputState MachineSimulator::pwm_output(PinAddress pin) const {
  const auto state = mbed::PwmOut::state(static_cast<PinName>((pin.port << 5u) | pin.pin));
  return PwmOutputState{
      state.configured,
      state.duty,
      state.period_us,
  };
}

void MachineSimulator::trigger_interrupt_rise(PinAddress pin) {
  mbed::InterruptIn::simulate_rise(static_cast<PinName>((pin.port << 5u) | pin.pin));
}

void MachineSimulator::set_adc_channel_raw(std::uint8_t channel, std::uint16_t raw12) {
  adc::set_channel_raw(channel, raw12);
}

std::uint16_t MachineSimulator::adc_channel_raw(std::uint8_t channel) const { return adc::channel_raw(channel); }

void MachineSimulator::set_temperature(TemperatureSensor sensor, double celsius) {
  set_adc_channel_raw(stock_temperature_channel(sensor), stock_beta_thermistor_raw(celsius));
}

void MachineSimulator::set_rotary_accessory_installed(bool installed) {
  rotary_accessory_installed_ = installed;
  constexpr std::size_t a_axis_index = 3;
  if (context_->stepper_axes().count() > a_axis_index) {
    context_->stepper_axes().set_motor_connected(a_axis_index, installed);
  }
}

std::size_t MachineSimulator::add_step_dir_axis(PinAddress step_pin, PinAddress direction_pin, bool invert_direction) {
  return context_->stepper_axes().add_axis(step_pin, direction_pin, invert_direction);
}

std::int64_t MachineSimulator::axis_position_steps(std::size_t axis) const {
  return context_->stepper_axes().position_steps(axis);
}

double MachineSimulator::axis_position_mm(std::size_t axis) const { return context_->stepper_axes().position_mm(axis); }

bool MachineSimulator::axis_endstop_triggered(std::size_t axis) const {
  return context_->stepper_axes().endstop_triggered(axis);
}

bool MachineSimulator::axis_endstop_triggered(std::size_t axis, EndstopSide side) const {
  return context_->stepper_axes().endstop_triggered(axis, side);
}

}  // namespace sim
