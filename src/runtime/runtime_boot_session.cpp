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

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/firm_config_data.hpp"
#include "sim/m8266_wifi.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_modules.hpp"

namespace sim {
namespace {

class FirmConfigDataScope {
 public:
  FirmConfigDataScope() { firm_config_data::set_enabled(true); }
  ~FirmConfigDataScope() { firm_config_data::set_enabled(false); }
};

void initialize_eeprom_for_power_on(const FactorySettings& settings) {
  if (i2c_eeprom::has_persistent_file() && i2c_eeprom::loaded_from_persistent_file()) {
    i2c_eeprom::reset_transaction();
    return;
  }

  i2c_eeprom::reset();
  i2c_eeprom::configure_factory_settings(settings);
}

void initialize_eeprom_for_reboot(const FactorySettings& settings) {
  if (i2c_eeprom::has_persistent_file()) {
    i2c_eeprom::reset_transaction();
    return;
  }

  i2c_eeprom::reset();
  i2c_eeprom::configure_factory_settings(settings);
}

}  // namespace

RuntimeBootSession::RuntimeBootSession(MachineSimulator& simulator, FactorySettings factory_settings)
    : simulator_(simulator), factory_settings_(factory_settings) {
  initialize_eeprom_for_power_on(factory_settings_);
  m8266_wifi::active().reset();
}

RuntimeBootSession::~RuntimeBootSession() = default;

Kernel& RuntimeBootSession::boot() {
  if (kernel_ != nullptr) {
    return *kernel_;
  }

  {
    FirmConfigDataScope firm_config_data_scope;
    kernel_ = std::make_unique<Kernel>();
  }
  runtime_modules::initialize_startup_gpio();
  const auto modules = runtime_modules::load_firmware_modules(*kernel_, simulator_, factory_settings_.machine_model);
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
  simulator_.reset(true);
  simulator_.set_realtime_speed(realtime_speed);
  if (was_realtime) {
    simulator_.start_realtime();
  }
  initialize_eeprom_for_reboot(factory_settings_);
  m8266_wifi::active().reset();
  homed_ = false;
}

void RuntimeBootSession::refresh_homed() {
  homed_ = kernel_ != nullptr && kernel_->robot != nullptr && kernel_->robot->is_homed_all_axes();
}

bool RuntimeBootSession::is_uploading() { return boot().is_uploading(); }

bool RuntimeBootSession::set_factory_settings(const FactorySettings& settings) {
  if (booted()) {
    return false;
  }

  factory_settings_ = settings;
  i2c_eeprom::reset();
  i2c_eeprom::configure_factory_settings(factory_settings_);
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
