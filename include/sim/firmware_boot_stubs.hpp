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

#ifndef SIMULATOR_FIRMWARE_BOOT_STUBS_HPP
#define SIMULATOR_FIRMWARE_BOOT_STUBS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <tuple>
#include <vector>

#include "ConfigValue.h"
#include "Module.h"
#include "PinNames.h"
#include "StreamOutputPool.h"
#include "sim/host_filesystem.hpp"

class StepperMotor;
class Block;

class ConfigValue;

class BootModule : public Module {
 public:
  void on_module_loaded() override { ++loaded_count; }

  static void reset_counts() { loaded_count = 0; }
  static int loaded_count;
};

class MSCFileSystem : public BootModule {
 public:
  explicit MSCFileSystem(const char*) {}
};

class SDFileSystem {
 public:
  SDFileSystem(PinName, PinName, PinName, PinName, int) {}
  int disk_initialize() { return disk_status; }

  static int disk_status;
};

class SDFAT {
 public:
  SDFAT(const char*, SDFileSystem*) {}
  int remount() {
    sim::host_filesystem::ensure_mount("sd");
    return 0;
  }
};

class SimMemoryPool {
 public:
  void* alloc(std::size_t bytes);
  void dealloc(void* ptr);
  std::uint32_t free() const;
  void debug(StreamOutput* stream) const;
};

inline void* operator new(std::size_t bytes, SimMemoryPool& pool) { return pool.alloc(bytes); }

extern SimMemoryPool simulator_ahb;

#define AHB simulator_ahb
#ifndef STACK_SIZE
#define STACK_SIZE 0
#endif

namespace sim::boot {

void reset_boot_stubs();
int loaded_module_count();

}  // namespace sim::boot

#endif
