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

#include "sim/runtime_modules.hpp"

#include "Config.h"
#include "ConfigValue.h"
#include "Conveyor.h"
#include "Module.h"
#include "Pin.h"
#include "Robot.h"
#include "SerialMessage.h"
#include "SlowTicker.h"
#include "StepTicker.h"
#include "StreamOutput.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "libs/Watchdog.h"
#include "libs/gpio.h"
#include "modules/communication/SerialConsole2.h"
#include "modules/tools/atc/ATCHandler.h"
#include "modules/tools/drillingcycles/Drillingcycles.h"
#include "modules/tools/endstops/Endstops.h"
#include "modules/tools/laser/Laser.h"
#include "modules/tools/spindle/SpindleMaker.h"
#include "modules/tools/switch/SwitchPool.h"
#include "modules/tools/temperaturecontrol/TemperatureControlPool.h"
#include "modules/tools/temperatureswitch/TemperatureSwitch.h"
#include "modules/tools/zprobe/ZProbe.h"
#include "modules/utils/mainbutton/MainButton.h"
#include "modules/utils/player/Player.h"
#include "modules/utils/simpleshell/SimpleShell.h"
#include "modules/utils/wifi/WifiProvider.h"
#include "sim/delay_hooks.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/motion_pump.hpp"
#include "sim/physical_scene.hpp"
#include "sim/robot_axis_binding.hpp"
#include "sim/runtime_atc_config.hpp"
#include "sim/runtime_checksums.hpp"
#include "sim/runtime_motor_alarm_wiring.hpp"
#include "sim/runtime_pin_config.hpp"
#include "sim/runtime_temperature.hpp"
#include "sim/spindle_state.hpp"
#include "sim/virtual_clock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

namespace sim::runtime_modules {

MachineModel machine_model_from_firmware(char model, MachineModel fallback);

namespace {

using runtime_pins::configured_pin;
using runtime_pins::default_e_stop_pin;
using runtime_pins::default_main_button_pin;
using runtime_pins::drive_configured_input;
using runtime_pins::pin_address;

std::size_t idle_motion_iterations() {
  const double speed = clock::active().realtime_speed();
  if (!clock::active().is_realtime() || !std::isfinite(speed) || speed <= 1.0) {
    return 1;
  }
  return std::clamp(static_cast<std::size_t>(std::ceil(speed)), std::size_t{1}, std::size_t{100});
}

// These bridge modules let the firmware's event loop observe simulated hardware
// side effects without teaching the firmware about the simulator.
class SimulatorTimerPumpBridgeModule : public Module {
 public:
  void on_module_loaded() override { register_for_event(ON_IDLE); }

  void on_idle(void* argument) override {
    if (THEKERNEL != nullptr) {
      if (argument != nullptr && !THEKERNEL->is_uploading()) {
        delay_hooks::run();
      }
      // Firmware blocking waits spin ON_IDLE; give emulated LPC timers a chance
      // to fire without shortcutting directly into firmware ticker callbacks.
      pump_motion(*THEKERNEL, idle_motion_iterations());
    }
  }
};

class SimulatorAtcPhysicalBridgeModule : public Module {
 public:
  void on_module_loaded() override {
    if (THEKERNEL != nullptr) {
      runtime_atc::configure_physical_scene(*THEKERNEL, true);
    }
    register_for_event(ON_GCODE_RECEIVED);
  }

  void on_gcode_received(void*) override {
    if (THEKERNEL != nullptr) {
      runtime_atc::configure_physical_scene(*THEKERNEL);
    }
  }
};

class SimulatorSpindleTachBridgeModule : public Module {
 public:
  explicit SimulatorSpindleTachBridgeModule(MachineSimulator& simulator) : simulator_(simulator) {}

  void on_module_loaded() override {
    if (THEKERNEL != nullptr && THEKERNEL->config != nullptr) {
      pwm_pin_ = configured_pin(*THEKERNEL, runtime_checksums::spindle, runtime_checksums::spindle_pwm_pin, "nc");
      pwm_output_inverted_ = pwm_pin_.is_inverting();
      max_pwm_ =
          THEKERNEL->config->value(runtime_checksums::spindle, runtime_checksums::spindle_max_pwm)->as_number(1.0F);
      feedback_pin_ =
          configured_pin(*THEKERNEL, runtime_checksums::spindle, runtime_checksums::spindle_feedback_pin, "nc");
      pulses_per_rev_ = THEKERNEL->config->value(runtime_checksums::spindle, runtime_checksums::spindle_pulses_per_rev)
                            ->as_number(2.0F);
      acc_ratio_ =
          THEKERNEL->config->value(runtime_checksums::spindle, runtime_checksums::spindle_acc_ratio)->as_number(1.0F);
      if (THEKERNEL->factory_set != nullptr) {
        model_ = machine_model_from_firmware(THEKERNEL->factory_set->MachineModel, model_);
      }
      if (model_ == MachineModel::CarveraAirCA1) {
        max_pwm_ = 0.9F;
      }
    }
    spindle_state::reset();
    last_pulse_us_ = simulator_.time_us();
    register_for_event(ON_IDLE);
  }

  void on_idle(void*) override {
    const auto now_us = simulator_.time_us();
    const double requested_rpm = requested_rpm_from_pwm();
    const bool enabled = requested_rpm > 0.5;
    spindle_state::update(model_, enabled, requested_rpm, now_us);
    const double actual_rpm = spindle_state::actual_rpm();
    if (!feedback_pin_.connected() || pulses_per_rev_ <= 0.0F || actual_rpm <= 0.5) {
      last_pulse_us_ = now_us;
      return;
    }

    const double pulse_interval_us = 60'000'000.0 * std::max(0.001F, acc_ratio_) / (actual_rpm * pulses_per_rev_);
    while (static_cast<double>(now_us - last_pulse_us_) >= pulse_interval_us) {
      simulator_.trigger_interrupt_rise(pin_address(feedback_pin_));
      last_pulse_us_ += static_cast<std::uint64_t>(pulse_interval_us);
    }
  }

 private:
  double requested_rpm_from_pwm() {
    if (!pwm_pin_.connected() || max_pwm_ <= 0.0F) {
      return 0.0;
    }

    const auto pwm = simulator_.pwm_output(pin_address(pwm_pin_));
    if (!pwm.configured) {
      return 0.0;
    }

    double duty = std::clamp(static_cast<double>(pwm.duty), 0.0, 1.0);
    if (pwm_output_inverted_) {
      duty = 1.0 - duty;
    }
    const double normalized_duty = std::clamp(duty / static_cast<double>(max_pwm_), 0.0, 1.0);
    return normalized_duty * spindle_state::max_rpm(model_);
  }

  MachineSimulator& simulator_;
  MachineModel model_{MachineModel::CarveraC1};
  Pin pwm_pin_;
  Pin feedback_pin_;
  bool pwm_output_inverted_{false};
  float max_pwm_{1.0F};
  float pulses_per_rev_{2.0F};
  float acc_ratio_{1.0F};
  std::uint64_t last_pulse_us_{0};
};

}  // namespace

MachineModel machine_model_from_firmware(char model, MachineModel fallback) {
  switch (model) {
    case 1:
      return MachineModel::CarveraC1;
    case 2:
      return MachineModel::CarveraAirCA1;
    default:
      return fallback;
  }
}

Module* make_atc_physical_module() { return new SimulatorAtcPhysicalBridgeModule(); }

Module* make_spindle_tach_module(MachineSimulator& simulator) {
  return new SimulatorSpindleTachBridgeModule(simulator);
}

void initialize_startup_gpio() {
  GPIO leds[] = {
      GPIO(P4_29),
      GPIO(P4_28),
      GPIO(P0_4),
      GPIO(P1_17),
  };
  for (auto& led : leds) {
    led.output();
    led = 0;
  }

  GPIO beep(P1_14);
  beep.output();
  beep = 0;
}

BootModules load_firmware_modules(Kernel& kernel, MachineSimulator& simulator, MachineModel model) {
  SimpleShell::version_command("", kernel.streams);
  attach_configured_stepper_axes(kernel, model, simulator.rotary_accessory_installed());
  runtime_motor_alarm_wiring::configure(kernel);
  kernel.add_module(new Player());
  kernel.add_module(make_atc_physical_module());
  kernel.add_module(new ATCHandler());

  BootModules modules;
  modules.wireless_probe_serial = new SerialConsole2();
  kernel.add_module(modules.wireless_probe_serial);

  drive_configured_input(simulator, kernel, runtime_checksums::main_button_pin, default_main_button_pin(model), false);
  drive_configured_input(simulator, kernel, runtime_checksums::e_stop_pin, default_e_stop_pin(model), false);
  kernel.add_module(new MainButton());

  kernel.add_module(new WifiProvider());

  SwitchPool switch_pool;
  switch_pool.load_tools();
  TemperatureControlPool temperature_pool;
  temperature_pool.load_tools();
  kernel.add_module(new Endstops());
  kernel.add_module(new Laser());
  SpindleMaker spindle_maker;
  spindle_maker.load_spindle();
  kernel.add_module(make_spindle_tach_module(simulator));
  kernel.add_module(new SimulatorTimerPumpBridgeModule());
  if (kernel.robot != nullptr && kernel.robot->get_number_registered_motors() >= 3) {
    kernel.add_module(new ZProbe());
  }
  simulator.set_temperature(TemperatureSensor::Spindle, 25.0);
  simulator.set_temperature(TemperatureSensor::Power, 25.0);
  runtime_temperature::warm_adc_filter();
  kernel.add_module(new TemperatureSwitch());
  kernel.add_module(new Drillingcycles());
  load_watchdog_if_enabled(kernel);
  replay_config_override(kernel);

  if (kernel.conveyor != nullptr && kernel.robot != nullptr) {
    kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  }
  if (kernel.step_ticker != nullptr) {
    kernel.step_ticker->start();
  }
  if (kernel.slow_ticker != nullptr) {
    kernel.slow_ticker->start();
  }
  return modules;
}

void load_watchdog_if_enabled(Kernel& kernel) {
  const float timeout_s = kernel.config->value(runtime_checksums::watchdog_timeout)->as_number(10.0F);
  if (timeout_s > 0.1F) {
    kernel.add_module(new Watchdog(timeout_s * 1'000'000.0F, WDT_RESET));
    kernel.streams->printf("Watchdog enabled for %1.3f seconds\n", timeout_s);
  } else {
    kernel.streams->printf("WARNING Watchdog is disabled\n");
  }
}

void replay_config_override(Kernel& kernel) {
  const auto path = host_filesystem::translate(kernel.config_override_filename());
  std::ifstream input(path);
  if (!input.is_open()) {
    return;
  }

  kernel.streams->printf("Loading config override file: %s...\n", kernel.config_override_filename());
  std::string line;
  while (std::getline(input, line)) {
    line.push_back('\n');
    kernel.streams->printf("  %s", line.c_str());
    if (!line.empty() && line.front() == ';') {
      continue;
    }
    SerialMessage message{&StreamOutput::NullStream, line, 0};
    kernel.call_event(ON_CONSOLE_LINE_RECEIVED, &message);
  }
  kernel.streams->printf("config override file executed\n");
}

}  // namespace sim::runtime_modules
