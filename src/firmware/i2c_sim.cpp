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

#include <algorithm>
#include <array>
#include <fstream>

#include "mbed.h"
#include "sim/crc16.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace {

constexpr std::uint16_t factory_page = 16;
constexpr std::uint16_t factory_page_size = 32;

}  // namespace

namespace sim {

void I2cEepromDevice::configure(I2cEepromConfig config) {
  if (config.size_bytes == 0) {
    config.size_bytes = i2c_eeprom::size;
  }
  if (config.page_size == 0) {
    config.page_size = 1;
  }

  config_ = config;
  storage_.assign(config_.size_bytes, config_.fill);
  if (persistent_path_.has_value()) {
    load_from_persistent_file();
  }
  stop();
  persist();
}

void I2cEepromDevice::reset() {
  storage_.assign(config_.size_bytes, config_.fill);
  stop();
  persist();
}

void I2cEepromDevice::reset_transaction() { stop(); }

bool I2cEepromDevice::use_persistent_file(const std::filesystem::path& path) {
  persistent_path_ = std::filesystem::absolute(path);
  loaded_from_persistent_file_ = false;
  load_from_persistent_file();
  if (!loaded_from_persistent_file_) {
    persist();
  }
  return loaded_from_persistent_file_;
}

void I2cEepromDevice::clear_persistent_file() {
  persistent_path_.reset();
  loaded_from_persistent_file_ = false;
}

bool I2cEepromDevice::has_persistent_file() const { return persistent_path_.has_value(); }

bool I2cEepromDevice::loaded_from_persistent_file() const { return loaded_from_persistent_file_; }

std::optional<std::filesystem::path> I2cEepromDevice::persistent_file() const { return persistent_path_; }

int I2cEepromDevice::select(std::uint8_t address) {
  if (address != config_.write_address && address != config_.read_address) {
    selected_ = false;
    return 1;
  }

  selected_ = true;
  read_mode_ = address == config_.read_address;
  address_bytes_seen_ = 0;
  return 0;
}

int I2cEepromDevice::write(std::uint8_t value) {
  if (!selected_) {
    return select(value);
  }

  if (read_mode_) {
    return 0;
  }

  if (address_bytes_seen_ == 0) {
    current_address_ = static_cast<std::uint16_t>(value << 8);
    ++address_bytes_seen_;
    return 0;
  }

  if (address_bytes_seen_ == 1) {
    current_address_ = static_cast<std::uint16_t>(current_address_ | value);
    page_start_address_ = static_cast<std::uint16_t>((current_address_ / config_.page_size) * config_.page_size);
    ++address_bytes_seen_;
    return 0;
  }

  poke(current_address_, value);
  current_address_ = wrapped_next_write_address();
  return 0;
}

int I2cEepromDevice::read() {
  const auto value = peek(current_address_);
  current_address_ = static_cast<std::uint16_t>(current_address_ + 1);
  return value;
}

void I2cEepromDevice::stop() {
  selected_ = false;
  read_mode_ = false;
  address_bytes_seen_ = 0;
}

void I2cEepromDevice::poke(std::uint16_t address, std::uint8_t value) {
  storage_[normalized_address(address)] = value;
  persist();
}

std::uint8_t I2cEepromDevice::peek(std::uint16_t address) const { return storage_[normalized_address(address)]; }

void I2cEepromDevice::configure_factory_settings(const FactorySettings& settings) {
  const auto address = static_cast<std::uint16_t>(factory_page * factory_page_size);
  std::array<std::uint8_t, 6> record{
      0x5a,
      0xa5,
      static_cast<std::uint8_t>(settings.machine_model),
      settings.function_setting,
      settings.reserve1,
      settings.reserve2,
  };
  const auto crc = crc16_ccitt(record.data(), record.size());

  for (std::size_t i = 0; i < record.size(); ++i) {
    poke(static_cast<std::uint16_t>(address + i), record[i]);
  }
  poke(static_cast<std::uint16_t>(address + record.size()), static_cast<std::uint8_t>(crc & 0xffu));
  poke(static_cast<std::uint16_t>(address + record.size() + 1), static_cast<std::uint8_t>(crc >> 8));
}

void I2cEepromDevice::load_from_persistent_file() {
  loaded_from_persistent_file_ = false;
  if (!persistent_path_.has_value()) {
    return;
  }

  std::ifstream input(*persistent_path_, std::ios::binary);
  if (!input) {
    return;
  }

  storage_.assign(config_.size_bytes, config_.fill);
  input.read(reinterpret_cast<char*>(storage_.data()), static_cast<std::streamsize>(storage_.size()));
  loaded_from_persistent_file_ = input.gcount() > 0;
}

void I2cEepromDevice::persist() const {
  if (!persistent_path_.has_value()) {
    return;
  }

  std::error_code error;
  std::filesystem::create_directories(persistent_path_->parent_path(), error);
  std::ofstream output(*persistent_path_, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(storage_.data()), static_cast<std::streamsize>(storage_.size()));
}

std::size_t I2cEepromDevice::normalized_address(std::uint16_t address) const {
  return storage_.empty() ? 0 : address % storage_.size();
}

std::uint16_t I2cEepromDevice::wrapped_next_write_address() const {
  const auto offset = static_cast<std::uint16_t>((current_address_ + 1 - page_start_address_) % config_.page_size);
  return static_cast<std::uint16_t>(page_start_address_ + offset);
}

}  // namespace sim

namespace sim::i2c_eeprom {

void configure(const I2cEepromConfig& config) { active().configure(config); }

void reset(std::uint8_t fill) {
  auto config = I2cEepromConfig{};
  config.fill = fill;
  active().configure(config);
}

void reset() { active().reset(); }

void reset_transaction() { active().reset_transaction(); }

bool use_persistent_file(const std::filesystem::path& path) { return active().use_persistent_file(path); }

void clear_persistent_file() { active().clear_persistent_file(); }

bool has_persistent_file() { return active().has_persistent_file(); }

bool loaded_from_persistent_file() { return active().loaded_from_persistent_file(); }

std::optional<std::filesystem::path> persistent_file() { return active().persistent_file(); }

void configure_factory_settings(const FactorySettings& settings) { active().configure_factory_settings(settings); }

void write(std::uint16_t address, std::uint8_t value) { active().poke(address, value); }

std::uint8_t read(std::uint16_t address) { return active().peek(address); }

I2cEepromDevice& active() { return compat::active_context().eeprom(); }

}  // namespace sim::i2c_eeprom

namespace mbed {

I2C::I2C(PinName, PinName) {}

void I2C::frequency(int hz) { frequency_hz_ = hz; }

void I2C::start() { sim::i2c_eeprom::active().stop(); }

int I2C::write(int value) { return sim::i2c_eeprom::active().write(static_cast<std::uint8_t>(value)); }

int I2C::read(int) { return sim::i2c_eeprom::active().read(); }

void I2C::stop() { sim::i2c_eeprom::active().stop(); }

}  // namespace mbed
