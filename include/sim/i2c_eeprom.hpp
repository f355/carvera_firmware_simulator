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

#ifndef SIMULATOR_SIM_I2C_EEPROM_HPP
#define SIMULATOR_SIM_I2C_EEPROM_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace sim::i2c_eeprom {

constexpr std::size_t size = 4096;

}  // namespace sim::i2c_eeprom

namespace sim {

enum class MachineModel : std::uint8_t {
  CarveraC1 = 1,
  CarveraAirCA1 = 2,
};

struct FactorySettings {
  MachineModel machine_model{MachineModel::CarveraC1};
  std::uint8_t function_setting{0x02};
  std::uint8_t reserve1{0};
  std::uint8_t reserve2{0};
};

struct I2cEepromConfig {
  std::size_t size_bytes{i2c_eeprom::size};
  std::size_t page_size{32};
  std::uint8_t fill{0xff};
  std::uint8_t write_address{0xa0};
  std::uint8_t read_address{0xa1};
};

class I2cEepromDevice {
 public:
  void configure(I2cEepromConfig config);
  void reset();
  void reset_transaction();
  bool use_persistent_file(const std::filesystem::path& path);
  void clear_persistent_file();
  bool has_persistent_file() const;
  bool loaded_from_persistent_file() const;
  std::optional<std::filesystem::path> persistent_file() const;

  int select(std::uint8_t address);
  int write(std::uint8_t value);
  int read();
  void stop();

  void poke(std::uint16_t address, std::uint8_t value);
  std::uint8_t peek(std::uint16_t address) const;
  void configure_factory_settings(const FactorySettings& settings);

 private:
  void load_from_persistent_file();
  void persist() const;
  std::size_t normalized_address(std::uint16_t address) const;
  std::uint16_t wrapped_next_write_address() const;

  I2cEepromConfig config_{};
  std::vector<std::uint8_t> storage_{std::vector<std::uint8_t>(i2c_eeprom::size, 0xff)};
  std::optional<std::filesystem::path> persistent_path_;
  bool loaded_from_persistent_file_{false};
  bool selected_{false};
  bool read_mode_{false};
  int address_bytes_seen_{0};
  std::uint16_t current_address_{0};
  std::uint16_t page_start_address_{0};
};

}  // namespace sim

namespace sim::i2c_eeprom {

void configure(const I2cEepromConfig& config);
void reset(std::uint8_t fill);
void reset();
void reset_transaction();
bool use_persistent_file(const std::filesystem::path& path);
void clear_persistent_file();
bool has_persistent_file();
bool loaded_from_persistent_file();
std::optional<std::filesystem::path> persistent_file();
void configure_factory_settings(const FactorySettings& settings);
void write(std::uint16_t address, std::uint8_t value);
std::uint8_t read(std::uint16_t address);
I2cEepromDevice& active();

}  // namespace sim::i2c_eeprom

#endif
