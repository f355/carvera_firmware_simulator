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

#include "sim/runtime_boot_session.hpp"

#include <cstdint>

#include "Config.h"
#include "Configurator.h"
#include "GcodeDispatch.h"
#include "Planner.h"
#include "Robot.h"
#include "SimpleShell.h"
#include "StepperMotor.h"
#include "lpc_memory_layout.hpp"
#include "libs/Kernel.h"
#include "sim/firm_config_data.hpp"
#include "sim/lpc_memory_constraints.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_modules.hpp"
#include "sim/simulator_context.hpp"
#include "sim/system_reset.hpp"

namespace sim {
namespace {

class FirmConfigDataScope {
 public:
  FirmConfigDataScope() { firm_config_data::set_enabled(true); }
  ~FirmConfigDataScope() { firm_config_data::set_enabled(false); }
};

void initialize_eeprom_for_power_on(I2cEepromDevice& eeprom, const FactorySettings& settings) {
  if (eeprom.has_persistent_file() && eeprom.loaded_from_persistent_file()) {
    eeprom.reset_transaction();
    return;
  }

  eeprom.reset();
  eeprom.configure_factory_settings(settings);
}

void initialize_eeprom_for_reboot(I2cEepromDevice& eeprom, const FactorySettings& settings) {
  if (eeprom.has_persistent_file()) {
    eeprom.reset_transaction();
    return;
  }

  eeprom.reset();
  eeprom.configure_factory_settings(settings);
}

template <typename Type>
void record_firmware_object(lpc_memory::MemoryAccounting& memory, Type* object, std::uint32_t target_bytes,
                            const char* type_name) {
  if (object == nullptr) {
    return;
  }
  memory.deallocate(object);
  memory.record_main(object, sizeof(Type), target_bytes, type_name);
}

}  // namespace

RuntimeBootSession::RuntimeBootSession(MachineSimulator& simulator, EventEngine& event_engine,
                                       FactorySettings factory_settings)
    : simulator_(simulator), event_engine_(event_engine), factory_settings_(factory_settings) {
  lpc_memory::apply_environment_defaults();
  lpc_memory::reset_for_reboot();
  initialize_eeprom_for_power_on(simulator_.context().eeprom(), factory_settings_);
  simulator_.context().m8266_wifi().reset();
}

RuntimeBootSession::~RuntimeBootSession() = default;

Kernel& RuntimeBootSession::boot() {
  if (kernel_ != nullptr) {
    return *kernel_;
  }
  if (boot_inhibited_) {
    // Explicit boot() is treated as a manual power-on retry after an abort.
    boot_inhibited_ = false;
  }

  {
    FirmConfigDataScope firm_config_data_scope;
    kernel_ = std::make_unique<Kernel>();
    auto& memory = simulator_.context().memory_accounting();
    record_firmware_object(memory, kernel_.get(), lpc_memory::generated::kKernelBytes, "Kernel");
    record_firmware_object(memory, kernel_->config, lpc_memory::generated::kConfigBytes, "Config");
    record_firmware_object(memory, kernel_->gcode_dispatch, lpc_memory::generated::kGcodeDispatchBytes,
                           "GcodeDispatch");
    record_firmware_object(memory, kernel_->robot, lpc_memory::generated::kRobotBytes, "Robot");
    record_firmware_object(memory, kernel_->planner, lpc_memory::generated::kPlannerBytes, "Planner");
    record_firmware_object(memory, kernel_->configurator, lpc_memory::generated::kConfiguratorBytes, "Configurator");
    record_firmware_object(memory, kernel_->simpleshell, lpc_memory::generated::kSimpleShellBytes, "SimpleShell");
    if (kernel_->robot != nullptr) {
      for (auto* motor : kernel_->robot->actuators) {
        record_firmware_object(memory, motor, lpc_memory::generated::kStepperMotorBytes, "StepperMotor");
      }
    }
  }
  runtime_modules::initialize_startup_gpio();
  const auto modules =
      runtime_modules::load_firmware_modules(*kernel_, simulator_, event_engine_, factory_settings_.machine_model);
  wireless_probe_serial_ = modules.wireless_probe_serial;

  return *kernel_;
}

void RuntimeBootSession::reset() {
  factory_settings_ = factory_settings();
  const bool was_realtime = simulator_.is_realtime();
  const double realtime_speed = simulator_.realtime_speed();
  kernel_.reset();
  wireless_probe_serial_ = nullptr;
  Kernel::instance = nullptr;
  boot_inhibited_ = false;
  lpc_memory::note_firmware_reboot();
  lpc_memory::reset_for_reboot();
  simulator_.reset(true);
  simulator_.set_realtime_speed(realtime_speed);
  if (was_realtime) {
    simulator_.start_realtime();
  }
  initialize_eeprom_for_reboot(simulator_.context().eeprom(), factory_settings_);
  simulator_.context().m8266_wifi().reset();
  homed_ = false;
}

void RuntimeBootSession::power_off() {
  kernel_.reset();
  wireless_probe_serial_ = nullptr;
  Kernel::instance = nullptr;
  boot_inhibited_ = true;
  homed_ = false;
  system_reset::consume_requested();
}

void RuntimeBootSession::refresh_homed() {
  homed_ = kernel_ != nullptr && kernel_->robot != nullptr && kernel_->robot->is_homed_all_axes();
}

bool RuntimeBootSession::is_uploading() { return kernel_ != nullptr && kernel_->is_uploading(); }

bool RuntimeBootSession::set_factory_settings(const FactorySettings& settings) {
  if (booted()) {
    return false;
  }

  factory_settings_ = settings;
  simulator_.context().eeprom().reset();
  simulator_.context().eeprom().configure_factory_settings(factory_settings_);
  return true;
}

FactorySettings RuntimeBootSession::factory_settings() const {
  if (kernel_ == nullptr || kernel_->factory_set == nullptr) {
    return factory_settings_;
  }

  return FactorySettings{
      runtime_modules::machine_model_from_firmware(kernel_->factory_set->MachineModel, factory_settings_.machine_model),
      static_cast<std::uint8_t>(kernel_->factory_set->FuncSetting),
      static_cast<std::uint8_t>(kernel_->factory_set->reserve1),
      static_cast<std::uint8_t>(kernel_->factory_set->reserve2),
  };
}

}  // namespace sim
