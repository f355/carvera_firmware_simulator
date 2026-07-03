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

#ifndef SIMULATOR_SIM_RUNTIME_MODULES_HPP
#define SIMULATOR_SIM_RUNTIME_MODULES_HPP

#include "sim/machine_simulator.hpp"

class Kernel;
class Module;
class SerialConsole2;

namespace sim {
class EventEngine;
}

namespace sim::runtime_modules {

struct BootModules {
  SerialConsole2* wireless_probe_serial{nullptr};
};

MachineModel machine_model_from_firmware(char model, MachineModel fallback);
Module* make_atc_physical_module(MachineSimulator& simulator);
Module* make_spindle_tach_module(MachineSimulator& simulator);
void initialize_startup_gpio();
BootModules load_firmware_modules(Kernel& kernel, MachineSimulator& simulator, EventEngine& event_engine,
                                  MachineModel model);
void load_watchdog_if_enabled(Kernel& kernel);
void replay_config_override(Kernel& kernel, MachineSimulator& simulator);

}  // namespace sim::runtime_modules

#endif
