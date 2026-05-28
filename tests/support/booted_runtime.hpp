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
#include "sim/firmware_runtime.hpp"
#include "sim/machine_simulator.hpp"

namespace sim::test {

class BootedRuntime {
 public:
  BootedRuntime() : runtime_(simulator_), kernel_(&runtime_.boot()) {}

  explicit BootedRuntime(const FactorySettings& factory_settings) : runtime_(simulator_) {
    runtime_.set_factory_settings(factory_settings);
    kernel_ = &runtime_.boot();
  }

  MachineSimulator& simulator() { return simulator_; }
  FirmwareRuntime& runtime() { return runtime_; }
  Kernel& kernel() { return *kernel_; }

 private:
  MachineSimulator simulator_;
  FirmwareRuntime runtime_;
  Kernel* kernel_{nullptr};
};

}  // namespace sim::test

#endif
