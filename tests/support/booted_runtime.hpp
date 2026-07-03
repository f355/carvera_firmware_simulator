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

#ifndef SIMULATOR_TESTS_SUPPORT_BOOTED_RUNTIME_HPP
#define SIMULATOR_TESTS_SUPPORT_BOOTED_RUNTIME_HPP

#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"

namespace sim::test {

class BootedRuntime {
 public:
  BootedRuntime() : kernel_(&simulation_.firmware().boot()) {}

  explicit BootedRuntime(const FactorySettings& factory_settings) {
    simulation_.firmware().set_factory_settings(factory_settings);
    kernel_ = &simulation_.firmware().boot();
  }

  MachineSimulator& simulator() { return simulation_.machine(); }
  FirmwareRuntime& runtime() { return simulation_.firmware(); }
  Kernel& kernel() { return *kernel_; }

 private:
  SimulationInstance simulation_;
  Kernel* kernel_{nullptr};
};

}  // namespace sim::test

#endif
