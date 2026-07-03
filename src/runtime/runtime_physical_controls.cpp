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

#include "sim/runtime_physical_controls.hpp"

#include "Config.h"
#include "ConfigValue.h"
#include "LaserPublicAccess.h"
#include "Pin.h"
#include "PublicData.h"
#include "libs/Kernel.h"
#include "modules/tools/endstops/EndstopsPublicAccess.h"
#include "modules/tools/switch/SwitchPublicAccess.h"
#include "sim/board_profile.hpp"
#include "sim/main_button_led.hpp"
#include "sim/physical_scene.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/runtime_checksums.hpp"
#include "sim/runtime_motor_alarm_wiring.hpp"
#include "sim/runtime_pin_config.hpp"
#include "sim/runtime_pump.hpp"
#include "sim/runtime_temperature.hpp"
#include "sim/simulator_context.hpp"

#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace sim {

namespace {

using runtime_pins::default_cover_pin;
using runtime_pins::default_e_stop_pin;
using runtime_pins::default_main_button_led_b_pin;
using runtime_pins::default_main_button_led_g_pin;
using runtime_pins::default_main_button_led_r_pin;
using runtime_pins::default_main_button_pin;
using runtime_pins::drive_configured_input;
using runtime_pins::pin_address;
using runtime_pins::raw_level_for_pin_get;
using runtime_pins::read_configured_input;
using runtime_pins::read_configured_output;

std::optional<std::uint16_t> switch_checksum_for(MachineSwitch name) {
  switch (name) {
    case MachineSwitch::Light:
      return light_checksum;
    case MachineSwitch::Vacuum:
      return vacuum_checksum;
    case MachineSwitch::SpindleFan:
      return spindlefan_checksum;
    case MachineSwitch::PowerFan:
      return powerfan_checksum;
    case MachineSwitch::ToolSensor:
      return toolsensor_checksum;
    case MachineSwitch::ProbeCharger:
      return probecharger_checksum;
    case MachineSwitch::Air:
      return air_checksum;
    case MachineSwitch::Beep:
      return beep_checksum;
  }
  return std::nullopt;
}

std::optional<MachineSwitch> fan_switch_for_temperature(TemperatureSensor sensor) {
  switch (sensor) {
    case TemperatureSensor::Power:
      return MachineSwitch::PowerFan;
    case TemperatureSensor::Spindle:
      return MachineSwitch::SpindleFan;
  }
  return std::nullopt;
}

float default_fan_value_for_temperature(TemperatureSensor sensor) {
  return sensor == TemperatureSensor::Power ? 30.0F : 50.0F;
}

}  // namespace

class RuntimePhysicalControls::PhysicalInputs {
 public:
  explicit PhysicalInputs(RuntimePhysicalControls& controls) : controls_(controls) {}

  void set_probe_inputs(bool probe, bool tool_setter) {
    controls_.boot_();
    const auto& profile = board_profile(controls_.machine_model());
    controls_.simulator_.set_gpio_input(profile.physical_probe.pin, signal_level(profile.physical_probe, probe));
    controls_.simulator_.set_gpio_input(profile.physical_tool_setter.pin,
                                        signal_level(profile.physical_tool_setter, tool_setter));
  }

  std::pair<bool, bool> probe_inputs() {
    controls_.boot_();
    const auto& profile = board_profile(controls_.machine_model());
    return {
        controls_.simulator_.gpio_level(profile.physical_probe.pin) == signal_level(profile.physical_probe, true),
        controls_.simulator_.gpio_level(profile.physical_tool_setter.pin) ==
            signal_level(profile.physical_tool_setter, true),
    };
  }

  void set_cover_open(bool open) {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr) {
      return;
    }

    Pin cover_pin;
    cover_pin.from_string(kernel.config->value(runtime_checksums::cover_endstop)
                              ->as_string(default_cover_pin(controls_.machine_model())));
    if (!cover_pin.connected()) {
      return;
    }

    controls_.simulator_.set_gpio_input(pin_address(cover_pin), raw_level_for_pin_get(cover_pin, !open));
  }

  bool cover_open() {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr) {
      return false;
    }

    Pin cover_pin;
    cover_pin.from_string(kernel.config->value(runtime_checksums::cover_endstop)
                              ->as_string(default_cover_pin(controls_.machine_model())));
    if (!cover_pin.connected()) {
      return false;
    }

    return !cover_pin.get();
  }

  void set_limit_switch(std::size_t axis, LimitSwitchSide side, bool triggered) {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr || axis >= std::size(runtime_checksums::limit_min)) {
      return;
    }

    const auto checksum =
        side == LimitSwitchSide::Min ? runtime_checksums::limit_min[axis] : runtime_checksums::limit_max[axis];
    drive_configured_input(controls_.simulator_, kernel, checksum, "nc", triggered);
  }

  bool limit_switch(std::size_t axis, LimitSwitchSide side) {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr || axis >= std::size(runtime_checksums::limit_min)) {
      return false;
    }

    const auto checksum =
        side == LimitSwitchSide::Min ? runtime_checksums::limit_min[axis] : runtime_checksums::limit_max[axis];
    return read_configured_input(kernel, checksum, "nc");
  }

  void set_main_button_pressed(bool pressed) {
    auto& kernel = controls_.boot_();
    drive_configured_input(controls_.simulator_, kernel, runtime_checksums::main_button_pin,
                           default_main_button_pin(controls_.machine_model()), pressed);
  }

  void set_e_stop_pressed(bool pressed) {
    auto& kernel = controls_.boot_();
    drive_configured_input(controls_.simulator_, kernel, runtime_checksums::e_stop_pin,
                           default_e_stop_pin(controls_.machine_model()), pressed);
  }

 private:
  RuntimePhysicalControls& controls_;
};

class RuntimePhysicalControls::FirmwareReadbacks {
 public:
  explicit FirmwareReadbacks(RuntimePhysicalControls& controls) : controls_(controls) {}

  FrontPanelState front_panel_state() {
    auto& kernel = controls_.boot_();
    FrontPanelState state;
    state.main_button_pressed = read_configured_input(kernel, runtime_checksums::main_button_pin,
                                                      default_main_button_pin(controls_.machine_model()));
    state.e_stop_pressed =
        read_configured_input(kernel, runtime_checksums::e_stop_pin, default_e_stop_pin(controls_.machine_model()));
    state.power_rails = controls_.simulator_.power_rails();

    bool available = false;
    state.direct_rgb.red = read_configured_output(kernel, runtime_checksums::main_button_led_r_pin,
                                                  default_main_button_led_r_pin(controls_.machine_model()), &available);
    state.direct_rgb.green =
        read_configured_output(kernel, runtime_checksums::main_button_led_g_pin,
                               default_main_button_led_g_pin(controls_.machine_model()), &available);
    state.direct_rgb.blue =
        read_configured_output(kernel, runtime_checksums::main_button_led_b_pin,
                               default_main_button_led_b_pin(controls_.machine_model()), &available);
    state.direct_rgb.available = available;
    state.led_strip.available =
        controls_.machine_model() == MachineModel::CarveraAirCA1 && main_button_led::available();
    const auto strip = main_button_led::strip();
    for (std::size_t i = 0; i < state.led_strip.segments.size(); ++i) {
      state.led_strip.segments[i] = RgbColorState{strip[i].red, strip[i].green, strip[i].blue};
    }
    return state;
  }

  bool set_switch_state(MachineSwitch name, bool on, std::optional<float> value) {
    controls_.boot_();
    const auto checksum = switch_checksum_for(name);
    if (!checksum.has_value()) {
      return false;
    }

    if (value.has_value()) {
      pad_switch state{
          static_cast<int>(*checksum),
          on,
          *value,
          0.0F,
      };
      return PublicData::set_value(switch_checksum, *checksum, state_value_checksum, &state);
    }

    bool state = on;
    return PublicData::set_value(switch_checksum, *checksum, state_checksum, &state);
  }

  SwitchState switch_state(MachineSwitch name) {
    controls_.boot_();
    const auto checksum = switch_checksum_for(name);
    if (!checksum.has_value()) {
      return {};
    }

    pad_switch state{};
    if (!PublicData::get_value(switch_checksum, *checksum, 0, &state)) {
      return {};
    }

    return SwitchState{
        true,
        state.state,
        state.value,
    };
  }

  LaserState laser_state() {
    controls_.boot_();
    laser_status status{};
    if (!PublicData::get_value(laser_checksum, get_laser_status_checksum, &status)) {
      return {};
    }

    return LaserState{
        true, status.mode, status.state, status.testing, status.power, status.scale,
    };
  }

 private:
  RuntimePhysicalControls& controls_;
};

class RuntimePhysicalControls::FaultInjection {
 public:
  explicit FaultInjection(RuntimePhysicalControls& controls) : controls_(controls) {}

  void set_motor_alarm(std::size_t axis, bool triggered) {
    controls_.boot_();
    controls_.simulator_.context().motor_alarm_wiring().drive(axis, triggered);
  }

  bool motor_alarm(std::size_t axis) {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr || axis >= std::size(runtime_checksums::motor_alarm)) {
      return false;
    }

    Pin alarm_pin;
    alarm_pin.from_string(kernel.config->value(runtime_checksums::motor_alarm[axis])->as_string("nc"));
    return alarm_pin.connected() && alarm_pin.get();
  }

  bool set_spindle_alarm(bool triggered) {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr) {
      return false;
    }

    Pin alarm_pin =
        runtime_pins::configured_pin(kernel, runtime_checksums::spindle, runtime_checksums::spindle_alarm_pin, "nc");
    if (!alarm_pin.connected()) {
      return false;
    }

    controls_.simulator_.set_gpio_input(pin_address(alarm_pin), raw_level_for_pin_get(alarm_pin, triggered));
    return true;
  }

  std::optional<bool> spindle_alarm() {
    auto& kernel = controls_.boot_();
    if (kernel.config == nullptr) {
      return std::nullopt;
    }

    Pin alarm_pin =
        runtime_pins::configured_pin(kernel, runtime_checksums::spindle, runtime_checksums::spindle_alarm_pin, "nc");
    if (!alarm_pin.connected()) {
      return std::nullopt;
    }

    return alarm_pin.get();
  }

  void set_temperature(TemperatureSensor sensor, double celsius) {
    auto& kernel = controls_.boot_();
    controls_.simulator_.set_temperature(sensor, celsius);
    runtime_temperature::warm_adc_filter();
    kernel.call_event(ON_SECOND_TICK);
    if (const auto fan = fan_switch_for_temperature(sensor)) {
      const auto state = controls_.firmware_readbacks_->switch_state(*fan);
      if (state.available && state.on && state.value <= 0.0F) {
        // Keep the physical fan output alive while firmware ADC oversampling
        // catches up with a host-side temperature jump.
        controls_.firmware_readbacks_->set_switch_state(*fan, true, default_fan_value_for_temperature(sensor));
      }
    }
    controls_.pump_.run_main_loop(1);
  }

 private:
  RuntimePhysicalControls& controls_;
};

RuntimePhysicalControls::RuntimePhysicalControls(MachineSimulator& simulator, RuntimeBootSession& boot_session,
                                                 RuntimePump& pump, BootCallback boot)
    : simulator_(simulator),
      boot_session_(boot_session),
      pump_(pump),
      boot_(std::move(boot)),
      physical_inputs_(std::make_unique<PhysicalInputs>(*this)),
      firmware_readbacks_(std::make_unique<FirmwareReadbacks>(*this)),
      fault_injection_(std::make_unique<FaultInjection>(*this)) {}

RuntimePhysicalControls::~RuntimePhysicalControls() = default;

MachineModel RuntimePhysicalControls::machine_model() const { return boot_session_.machine_model(); }

void RuntimePhysicalControls::set_probe_inputs(bool probe, bool tool_setter) {
  physical_inputs_->set_probe_inputs(probe, tool_setter);
}

std::pair<bool, bool> RuntimePhysicalControls::probe_inputs() { return physical_inputs_->probe_inputs(); }

void RuntimePhysicalControls::set_cover_open(bool open) { physical_inputs_->set_cover_open(open); }

bool RuntimePhysicalControls::cover_open() { return physical_inputs_->cover_open(); }

void RuntimePhysicalControls::set_limit_switch(std::size_t axis, LimitSwitchSide side, bool triggered) {
  physical_inputs_->set_limit_switch(axis, side, triggered);
}

bool RuntimePhysicalControls::limit_switch(std::size_t axis, LimitSwitchSide side) {
  return physical_inputs_->limit_switch(axis, side);
}

void RuntimePhysicalControls::set_motor_alarm(std::size_t axis, bool triggered) {
  fault_injection_->set_motor_alarm(axis, triggered);
}

bool RuntimePhysicalControls::motor_alarm(std::size_t axis) { return fault_injection_->motor_alarm(axis); }

bool RuntimePhysicalControls::set_spindle_alarm(bool triggered) {
  return fault_injection_->set_spindle_alarm(triggered);
}

std::optional<bool> RuntimePhysicalControls::spindle_alarm() { return fault_injection_->spindle_alarm(); }

void RuntimePhysicalControls::set_probe_tool_installed(bool installed) {
  simulator_.context().physical_scene().set_probe_tool_installed(installed);
}

void RuntimePhysicalControls::set_tool_setter_box(const Box& box) {
  simulator_.context().physical_scene().set_tool_setter_box(box);
}

void RuntimePhysicalControls::set_stock_box(const Box& box) { simulator_.context().physical_scene().set_stock_box(box); }

void RuntimePhysicalControls::set_main_button_pressed(bool pressed) {
  physical_inputs_->set_main_button_pressed(pressed);
}

void RuntimePhysicalControls::set_e_stop_pressed(bool pressed) { physical_inputs_->set_e_stop_pressed(pressed); }

FrontPanelState RuntimePhysicalControls::front_panel_state() { return firmware_readbacks_->front_panel_state(); }

void RuntimePhysicalControls::set_temperature(TemperatureSensor sensor, double celsius) {
  fault_injection_->set_temperature(sensor, celsius);
}

bool RuntimePhysicalControls::set_switch_state(MachineSwitch name, bool on, std::optional<float> value) {
  return firmware_readbacks_->set_switch_state(name, on, value);
}

SwitchState RuntimePhysicalControls::switch_state(MachineSwitch name) {
  return firmware_readbacks_->switch_state(name);
}

LaserState RuntimePhysicalControls::laser_state() { return firmware_readbacks_->laser_state(); }

}  // namespace sim
