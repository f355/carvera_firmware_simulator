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

#include "libs/Kernel.h"
#include "sim/host_filesystem.hpp"
#include "sim/i2c_eeprom.hpp"
#include "support/assertions.hpp"

int main() {
  using sim::test::require;

  sim::host_filesystem::clear_mounts();

  sim::i2c_eeprom::reset();
  sim::i2c_eeprom::configure_factory_settings(sim::FactorySettings{});

  {
    Kernel kernel;
    require(kernel.factory_set->MachineModel == CARVERA, "Default factory settings should select the C1 model");
    require((kernel.factory_set->FuncSetting & 0x02) != 0,
            "Default factory settings should use the non-gap-check rotary homing flag");
    require((kernel.factory_set->FuncSetting & 0x01) == 0,
            "Default factory settings should not use the rotary gap-check homing flag");
  }

  sim::i2c_eeprom::reset();
  sim::i2c_eeprom::configure_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraAirCA1, 0x00});

  {
    Kernel kernel;
    require(kernel.factory_set->MachineModel == CARVERA_AIR,
            "Kernel should read the CA1 model from simulated EEPROM factory data");
    require(kernel.factory_set->FuncSetting == 0x00, "Kernel should preserve CA1 function flags from simulated EEPROM");
  }

  sim::i2c_eeprom::reset();
  sim::i2c_eeprom::configure_factory_settings(sim::FactorySettings{sim::MachineModel::CarveraC1, 0x00});

  {
    Kernel kernel;
    require(kernel.factory_set->MachineModel == CARVERA,
            "Kernel should read the C1 model from simulated EEPROM factory data");
    require((kernel.factory_set->FuncSetting & 0x04) != 0,
            "Kernel should apply firmware's C1 ATC function flag normalization");
  }

  return 0;
}
