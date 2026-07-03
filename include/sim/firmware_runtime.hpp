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

#ifndef SIMULATOR_SIM_FIRMWARE_RUNTIME_HPP
#define SIMULATOR_SIM_FIRMWARE_RUNTIME_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sim/i2c_eeprom.hpp"
#include "sim/runtime_physical_types.hpp"
#include "sim/runtime_pump.hpp"

class Kernel;

namespace sim {

class RuntimeBootSession;
class RuntimeIo;
class RuntimePhysicalControls;

class FirmwareRuntime {
 public:
  explicit FirmwareRuntime(MachineSimulator& simulator);
  ~FirmwareRuntime();

  Kernel& start();
  Kernel& boot();
  void reset();
  bool booted() const;
  bool is_homed() const;
  bool is_uploading();
  void home_machine();
  bool set_factory_settings(const FactorySettings& settings);
  FactorySettings factory_settings() const;
  void set_probe_inputs(bool probe, bool tool_setter);
  std::pair<bool, bool> probe_inputs();
  void set_cover_open(bool open);
  bool cover_open();
  void set_limit_switch(std::size_t axis, LimitSwitchSide side, bool triggered);
  bool limit_switch(std::size_t axis, LimitSwitchSide side);
  void set_motor_alarm(std::size_t axis, bool triggered);
  bool motor_alarm(std::size_t axis);
  bool set_spindle_alarm(bool triggered);
  std::optional<bool> spindle_alarm();
  void set_probe_tool_installed(bool installed);
  void set_tool_setter_box(const Box& box);
  void set_stock_box(const Box& box);
  void set_main_button_pressed(bool pressed);
  void set_e_stop_pressed(bool pressed);
  FrontPanelState front_panel_state();
  void set_temperature(TemperatureSensor sensor, double celsius);
  bool set_switch_state(MachineSwitch name, bool on, std::optional<float> value = std::nullopt);
  SwitchState switch_state(MachineSwitch name);
  LaserState laser_state();

  void write_serial(const std::string& data);
  std::string read_serial();
  void write_wifi_tcp(const std::string& data);
  std::string read_wifi_tcp();
  void set_wifi_client_connected(bool connected);
  std::vector<std::string> take_wifi_udp_datagrams();
  void write_wireless_probe_rx(const std::string& data);
  std::string read_wireless_probe_tx();
  RuntimePumpResult pump(const RuntimePumpOptions& options);
  void run_main_loop(std::size_t iterations);
  bool run_until_idle(std::size_t max_step_ticks);
  bool pump_free_running(std::size_t main_loop_iterations = 4, std::size_t max_step_ticks = 1000);
  double realtime_speed() const;

 private:
  MachineModel machine_model() const;

  MachineSimulator& simulator_;
  std::unique_ptr<RuntimeBootSession> boot_session_;
  std::unique_ptr<RuntimePump> pump_;
  std::unique_ptr<RuntimeIo> io_;
  std::unique_ptr<RuntimePhysicalControls> physical_controls_;
};

}  // namespace sim

#endif
