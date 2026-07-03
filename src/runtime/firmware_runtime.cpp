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

#include "sim/firmware_runtime.hpp"

#include "Gcode.h"
#include "Robot.h"
#include "StreamOutput.h"
#include "libs/Kernel.h"
#include "sim/event_engine.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/runtime_io.hpp"
#include "sim/runtime_physical_controls.hpp"
#include "sim/runtime_pump.hpp"

namespace sim {

namespace {

constexpr FactorySettings default_factory_settings{MachineModel::CarveraC1, 0x04};

}  // namespace

FirmwareRuntime::FirmwareRuntime(MachineSimulator& simulator)
    : simulator_(simulator),
      event_engine_(std::make_unique<EventEngine>(simulator_)),
      boot_session_(std::make_unique<RuntimeBootSession>(simulator_, *event_engine_, default_factory_settings)),
      pump_(std::make_unique<RuntimePump>(*event_engine_, *boot_session_)),
      io_(std::make_unique<RuntimeIo>(simulator_, *boot_session_, [this]() -> Kernel& { return start(); })),
      physical_controls_(std::make_unique<RuntimePhysicalControls>(simulator_, *boot_session_, *pump_,
                                                                   [this]() -> Kernel& { return start(); })) {}

FirmwareRuntime::~FirmwareRuntime() = default;

Kernel& FirmwareRuntime::start() { return boot_session_->boot(); }

Kernel& FirmwareRuntime::boot() {
  const bool already_booted = boot_session_->booted();
  auto& kernel = boot_session_->boot();
  if (!already_booted) {
    pump_->run_until_motion_idle(200'000);
    boot_session_->refresh_homed();
  }
  return kernel;
}

void FirmwareRuntime::reset() { boot_session_->reset(); }

bool FirmwareRuntime::booted() const { return boot_session_->booted(); }

bool FirmwareRuntime::is_homed() const { return boot_session_->is_homed(); }

void FirmwareRuntime::home_machine() {
  auto& kernel = boot();
  if (kernel.robot == nullptr) {
    return;
  }

  Gcode gcode(kernel.is_grbl_mode() ? "G28.2" : "G28", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &gcode);
  boot_session_->refresh_homed();
}

bool FirmwareRuntime::set_factory_settings(const FactorySettings& settings) {
  return boot_session_->set_factory_settings(settings);
}

FactorySettings FirmwareRuntime::factory_settings() const { return boot_session_->factory_settings(); }

MachineModel FirmwareRuntime::machine_model() const { return boot_session_->machine_model(); }

bool FirmwareRuntime::is_uploading() { return start().is_uploading(); }

void FirmwareRuntime::set_probe_inputs(bool probe, bool tool_setter) {
  physical_controls_->set_probe_inputs(probe, tool_setter);
}

std::pair<bool, bool> FirmwareRuntime::probe_inputs() { return physical_controls_->probe_inputs(); }

void FirmwareRuntime::set_cover_open(bool open) { physical_controls_->set_cover_open(open); }

bool FirmwareRuntime::cover_open() { return physical_controls_->cover_open(); }

void FirmwareRuntime::set_limit_switch(std::size_t axis, LimitSwitchSide side, bool triggered) {
  physical_controls_->set_limit_switch(axis, side, triggered);
}

bool FirmwareRuntime::limit_switch(std::size_t axis, LimitSwitchSide side) {
  return physical_controls_->limit_switch(axis, side);
}

void FirmwareRuntime::set_motor_alarm(std::size_t axis, bool triggered) {
  physical_controls_->set_motor_alarm(axis, triggered);
}

bool FirmwareRuntime::motor_alarm(std::size_t axis) { return physical_controls_->motor_alarm(axis); }

bool FirmwareRuntime::set_spindle_alarm(bool triggered) { return physical_controls_->set_spindle_alarm(triggered); }

std::optional<bool> FirmwareRuntime::spindle_alarm() { return physical_controls_->spindle_alarm(); }

void FirmwareRuntime::set_probe_tool_installed(bool installed) {
  physical_controls_->set_probe_tool_installed(installed);
}

void FirmwareRuntime::set_tool_setter_box(const Box& box) { physical_controls_->set_tool_setter_box(box); }

void FirmwareRuntime::set_stock_box(const Box& box) { physical_controls_->set_stock_box(box); }

void FirmwareRuntime::set_main_button_pressed(bool pressed) { physical_controls_->set_main_button_pressed(pressed); }

void FirmwareRuntime::set_e_stop_pressed(bool pressed) { physical_controls_->set_e_stop_pressed(pressed); }

FrontPanelState FirmwareRuntime::front_panel_state() { return physical_controls_->front_panel_state(); }

void FirmwareRuntime::set_temperature(TemperatureSensor sensor, double celsius) {
  physical_controls_->set_temperature(sensor, celsius);
}

bool FirmwareRuntime::set_switch_state(MachineSwitch name, bool on, std::optional<float> value) {
  return physical_controls_->set_switch_state(name, on, value);
}

SwitchState FirmwareRuntime::switch_state(MachineSwitch name) { return physical_controls_->switch_state(name); }

LaserState FirmwareRuntime::laser_state() { return physical_controls_->laser_state(); }

void FirmwareRuntime::write_serial(const std::string& data) { io_->write_serial(data); }

std::string FirmwareRuntime::read_serial() { return io_->read_serial(); }

void FirmwareRuntime::write_wifi_tcp(const std::string& data) { io_->write_wifi_tcp(data); }

std::string FirmwareRuntime::read_wifi_tcp() { return io_->read_wifi_tcp(); }

void FirmwareRuntime::set_wifi_client_connected(bool connected) { io_->set_wifi_client_connected(connected); }

std::vector<std::string> FirmwareRuntime::take_wifi_udp_datagrams() { return io_->take_wifi_udp_datagrams(); }

void FirmwareRuntime::write_wireless_probe_rx(const std::string& data) { io_->write_wireless_probe_rx(data); }

std::string FirmwareRuntime::read_wireless_probe_tx() { return io_->read_wireless_probe_tx(); }

RuntimePumpResult FirmwareRuntime::pump(const RuntimePumpOptions& options) { return pump_->pump(options); }

void FirmwareRuntime::run_main_loop(std::size_t iterations) { pump_->run_main_loop(iterations); }

EventRunResult FirmwareRuntime::run_until_motion_idle(std::size_t max_timer_events) {
  return pump_->run_until_motion_idle(max_timer_events);
}

bool FirmwareRuntime::run_until_idle(std::size_t max_step_ticks) {
  return run_until_motion_idle(max_step_ticks).motion_idle;
}

bool FirmwareRuntime::pump_free_running(std::size_t main_loop_iterations, std::size_t max_step_ticks) {
  return pump_->pump_free_running(main_loop_iterations, max_step_ticks);
}

double FirmwareRuntime::realtime_speed() const { return simulator_.realtime_speed(); }

RuntimePhysicalControls& FirmwareRuntime::inputs() { return *physical_controls_; }

RuntimeIo& FirmwareRuntime::io() { return *io_; }

RuntimePump& FirmwareRuntime::runner() { return *pump_; }

}  // namespace sim
