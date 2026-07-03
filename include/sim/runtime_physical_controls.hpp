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

#ifndef SIMULATOR_SIM_RUNTIME_PHYSICAL_CONTROLS_HPP
#define SIMULATOR_SIM_RUNTIME_PHYSICAL_CONTROLS_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "sim/runtime_physical_types.hpp"

class Kernel;

namespace sim {

class MachineSimulator;
class RuntimeBootSession;
class RuntimePump;

class RuntimePhysicalControls {
 public:
  using BootCallback = std::function<Kernel&()>;

  RuntimePhysicalControls(MachineSimulator& simulator, RuntimeBootSession& boot_session, RuntimePump& pump,
                          BootCallback boot);
  ~RuntimePhysicalControls();

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
  void set_main_button_pressed(bool pressed);
  void set_e_stop_pressed(bool pressed);
  FrontPanelState front_panel_state();
  void set_temperature(TemperatureSensor sensor, double celsius);
  bool set_switch_state(MachineSwitch name, bool on, std::optional<float> value = std::nullopt);
  SwitchState switch_state(MachineSwitch name);
  LaserState laser_state();

 private:
  class PhysicalInputs;
  class FaultInjection;
  class FirmwareReadbacks;

  MachineModel machine_model() const;

  MachineSimulator& simulator_;
  RuntimeBootSession& boot_session_;
  RuntimePump& pump_;
  BootCallback boot_;
  std::unique_ptr<PhysicalInputs> physical_inputs_;
  std::unique_ptr<FirmwareReadbacks> firmware_readbacks_;
  std::unique_ptr<FaultInjection> fault_injection_;
};

}  // namespace sim

#endif
