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

#include "sim/event_engine.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/runtime_io.hpp"
#include "sim/runtime_physical_controls.hpp"
#include "sim/runtime_pump.hpp"

class Kernel;

namespace sim {

class MachineSimulator;

class FirmwareRuntime {
 public:
  explicit FirmwareRuntime(MachineSimulator& simulator);
  ~FirmwareRuntime();

  Kernel& start();
  Kernel& boot();
  void reset();
  void power_off();
  bool booted() const;
  bool boot_inhibited() const;
  bool is_homed() const;
  bool is_uploading();
  void home_machine();
  bool set_factory_settings(const FactorySettings& settings);
  FactorySettings factory_settings() const;

  RuntimePhysicalControls& inputs();
  RuntimeIo& io();
  RuntimePump& runner();

 private:
  EventEngine event_engine_;
  RuntimeBootSession boot_session_;
  RuntimePump pump_;
  RuntimeIo io_;
  RuntimePhysicalControls physical_controls_;
};

}  // namespace sim

#endif
