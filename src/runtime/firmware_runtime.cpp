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

#include "sim/firmware_runtime.hpp"

#include "Gcode.h"
#include "Robot.h"
#include "StreamOutput.h"
#include "libs/Kernel.h"
#include "sim/event_engine.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_boot_session.hpp"
#include "sim/runtime_io.hpp"
#include "sim/runtime_physical_controls.hpp"
#include "sim/runtime_pump.hpp"

namespace sim {

namespace {

constexpr FactorySettings default_factory_settings{MachineModel::CarveraC1, 0x04};

}  // namespace

FirmwareRuntime::FirmwareRuntime(MachineSimulator& simulator)
    : event_engine_(simulator),
      boot_session_(simulator, event_engine_, default_factory_settings),
      pump_(simulator, event_engine_, boot_session_),
      io_(simulator, boot_session_, [this]() -> Kernel& { return start(); }),
      physical_controls_(simulator, boot_session_, pump_, [this]() -> Kernel& { return start(); }) {}

FirmwareRuntime::~FirmwareRuntime() = default;

Kernel& FirmwareRuntime::start() { return boot_session_.boot(); }

Kernel& FirmwareRuntime::boot() {
  const bool already_booted = boot_session_.booted();
  auto& kernel = boot_session_.boot();
  if (!already_booted) {
    pump_.run_until_motion_idle(200'000);
    boot_session_.refresh_homed();
  }
  return kernel;
}

void FirmwareRuntime::reset() { boot_session_.reset(); }

bool FirmwareRuntime::booted() const { return boot_session_.booted(); }

bool FirmwareRuntime::is_homed() const { return boot_session_.is_homed(); }

void FirmwareRuntime::home_machine() {
  auto& kernel = boot();
  if (kernel.robot == nullptr) {
    return;
  }

  Gcode gcode(kernel.is_grbl_mode() ? "G28.2" : "G28", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &gcode);
  boot_session_.refresh_homed();
}

bool FirmwareRuntime::set_factory_settings(const FactorySettings& settings) {
  return boot_session_.set_factory_settings(settings);
}

FactorySettings FirmwareRuntime::factory_settings() const { return boot_session_.factory_settings(); }

bool FirmwareRuntime::is_uploading() { return start().is_uploading(); }

RuntimePhysicalControls& FirmwareRuntime::inputs() { return physical_controls_; }

RuntimeIo& FirmwareRuntime::io() { return io_; }

RuntimePump& FirmwareRuntime::runner() { return pump_; }

}  // namespace sim
