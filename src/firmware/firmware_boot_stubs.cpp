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

#include "sim/firmware_boot_stubs.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string_view>

#include "Block.h"
#include "Conveyor.h"
#include "Gcode.h"
#include "Robot.h"
#include "SerialMessage.h"
#include "SlowTicker.h"
#include "StepTicker.h"
#include "StreamOutput.h"
#include "gpio.h"
#include "lpc_memory_layout.hpp"
#include "libs/Kernel.h"
#include "modules/communication/SerialConsole.h"
#include "modules/communication/SerialConsole2.h"
#include "compat/active_context.hpp"
#include "sim/lpc_memory_accounting.hpp"
#include "sim/simulator_context.hpp"

int BootModule::loaded_count = 0;
int SDFileSystem::disk_status = 1;
SimMemoryPool simulator_ahb;
#ifdef _WIN32
#define SIM_WEAK_FIRMWARE_GLOBAL
#else
#define SIM_WEAK_FIRMWARE_GLOBAL __attribute__((weak))
#endif
SDFAT mounter SIM_WEAK_FIRMWARE_GLOBAL("sd", nullptr);
GPIO leds[4] SIM_WEAK_FIRMWARE_GLOBAL = {
    GPIO(P4_29),
    GPIO(P4_28),
    GPIO(P0_4),
    GPIO(P1_17),
};
#undef SIM_WEAK_FIRMWARE_GLOBAL

namespace {

struct AhbTypeLayout {
  std::size_t host_bytes;
  std::uint32_t target_bytes;
  const char* name;
};

std::tuple<std::size_t, std::string, bool> target_ahb_payload(std::size_t host_bytes) {
  if (host_bytes >= 2 * sizeof(Block) && host_bytes % sizeof(Block) == 0) {
    return {
        (host_bytes / sizeof(Block)) * sim::lpc_memory::generated::kBlockBytes,
        "Block[]",
        true,
    };
  }

  constexpr std::array layouts{
      AhbTypeLayout{sizeof(Conveyor), sim::lpc_memory::generated::kConveyorBytes, "Conveyor"},
      AhbTypeLayout{sizeof(SerialConsole), sim::lpc_memory::generated::kSerialConsoleBytes, "SerialConsole"},
      AhbTypeLayout{sizeof(SerialConsole2), sim::lpc_memory::generated::kSerialConsole2Bytes, "SerialConsole2"},
      AhbTypeLayout{sizeof(SlowTicker), sim::lpc_memory::generated::kSlowTickerBytes, "SlowTicker"},
      AhbTypeLayout{sizeof(StepTicker), sim::lpc_memory::generated::kStepTickerBytes, "StepTicker"},
  };
  const AhbTypeLayout* match = nullptr;
  for (const auto& layout : layouts) {
    if (layout.host_bytes != host_bytes) {
      continue;
    }
    if (match != nullptr &&
        (match->target_bytes != layout.target_bytes || std::string_view(match->name) != layout.name)) {
      return {host_bytes, "ABI-unresolved", false};
    }
    match = &layout;
  }
  if (match != nullptr) {
    return {match->target_bytes, match->name, true};
  }
  // Every remaining direct AHB.alloc() call in the pinned firmware is a byte
  // or float buffer, whose requested byte count is identical on the LPC.
  return {host_bytes, "raw buffer", true};
}

}  // namespace

void* SimMemoryPool::alloc(std::size_t bytes) {
  void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
  if (pointer == nullptr) {
    return nullptr;
  }
  try {
    if (auto* context = sim::compat::try_active_context(); context != nullptr) {
      const auto [target_bytes, type_name, exact] = target_ahb_payload(bytes);
      context->memory_accounting().record_ahb(pointer, bytes, target_bytes, type_name, exact);
    }
  } catch (...) {
  }
  return pointer;
}

void SimMemoryPool::dealloc(void* pointer) {
  sim::lpc_memory::release_host_allocation(pointer);
  std::free(pointer);
}

std::uint32_t SimMemoryPool::free() const {
  if (auto* context = sim::compat::try_active_context(); context != nullptr) {
    return context->memory_accounting().snapshot().ahb.total_free_bytes;
  }
  return 0;
}

void SimMemoryPool::debug(StreamOutput* stream) const {
  if (stream == nullptr) {
    return;
  }
  auto* context = sim::compat::try_active_context();
  if (context == nullptr) {
    stream->printf("AHB accounting unavailable\n");
    return;
  }
  const auto snapshot = context->memory_accounting().snapshot().ahb;
  stream->printf("AHB static=%lu dynamic=%lu live=%lu peak=%lu overhead=%lu largest-free=%lu failures=%lu\n",
                 static_cast<unsigned long>(snapshot.static_bytes),
                 static_cast<unsigned long>(snapshot.dynamic_capacity_bytes),
                 static_cast<unsigned long>(snapshot.live_payload_bytes),
                 static_cast<unsigned long>(snapshot.peak_live_payload_bytes),
                 static_cast<unsigned long>(snapshot.allocator_overhead_bytes),
                 static_cast<unsigned long>(snapshot.largest_free_block_bytes),
                 static_cast<unsigned long>(snapshot.failed_allocation_count));
}

namespace sim::boot {

void reset_boot_stubs() {
  BootModule::reset_counts();
  SDFileSystem::disk_status = 1;
}

int loaded_module_count() { return BootModule::loaded_count; }

}  // namespace sim::boot
