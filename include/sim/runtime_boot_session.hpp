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

#ifndef SIMULATOR_SIM_RUNTIME_BOOT_SESSION_HPP
#define SIMULATOR_SIM_RUNTIME_BOOT_SESSION_HPP

#include <memory>

#include "sim/i2c_eeprom.hpp"

class Kernel;
class SerialConsole2;

namespace sim {

class MachineSimulator;
class EventEngine;

class RuntimeBootSession {
 public:
  RuntimeBootSession(MachineSimulator& simulator, EventEngine& event_engine, FactorySettings factory_settings);
  ~RuntimeBootSession();

  Kernel& boot();
  void reset();
  bool booted() const { return kernel_ != nullptr; }

  bool is_homed() const { return homed_; }
  void refresh_homed();
  bool is_uploading();

  bool set_factory_settings(const FactorySettings& settings);
  FactorySettings factory_settings() const;
  MachineModel machine_model() const { return factory_settings().machine_model; }

  SerialConsole2* wireless_probe_serial() const { return wireless_probe_serial_; }

 private:
  MachineSimulator& simulator_;
  EventEngine& event_engine_;
  std::unique_ptr<Kernel> kernel_;
  FactorySettings factory_settings_;
  SerialConsole2* wireless_probe_serial_{nullptr};
  bool homed_{false};
};

}  // namespace sim

#endif
