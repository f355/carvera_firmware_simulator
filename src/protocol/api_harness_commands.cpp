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

#include "sim/api_service.hpp"

#include <array>
#include <cstddef>
#include <exception>
#include <string>

#include "libs/Kernel.h"
#include "sim/api_conversions.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/persistent_machine_state.hpp"

namespace sim {
namespace {

constexpr std::uint16_t eeprom_data_offset = 32;
constexpr std::size_t tool_not_calibrated_offset = offsetof(EEPROM_data, tool_not_calibrated);
constexpr int persistent_variable_base = 501;
constexpr int persistent_variable_count = 20;
constexpr int work_coordinate_system_base = 54;
constexpr int work_coordinate_system_count = 6;
static_assert(sizeof(bool) == 1, "firmware EEPROM layout requires one-byte bools");

EEPROM_data read_persistent_data(const I2cEepromDevice& eeprom) {
  EEPROM_data data{};
  auto* output = reinterpret_cast<std::uint8_t*>(&data);
  for (std::size_t offset = 0; offset < sizeof(EEPROM_data); ++offset) {
    if (offset == tool_not_calibrated_offset) {
      continue;
    }
    output[offset] = eeprom.peek(static_cast<std::uint16_t>(eeprom_data_offset + offset));
  }
  data.tool_not_calibrated =
      eeprom.peek(static_cast<std::uint16_t>(eeprom_data_offset + tool_not_calibrated_offset)) != 0;
  return data;
}

void write_persistent_data(I2cEepromDevice& eeprom, const EEPROM_data& data) {
  const auto* input = reinterpret_cast<const std::uint8_t*>(&data);
  for (std::size_t offset = 0; offset < sizeof(EEPROM_data); ++offset) {
    if (offset == tool_not_calibrated_offset) {
      continue;
    }
    eeprom.poke(static_cast<std::uint16_t>(eeprom_data_offset + offset), input[offset]);
  }
  eeprom.poke(static_cast<std::uint16_t>(eeprom_data_offset + tool_not_calibrated_offset),
              data.tool_not_calibrated ? 1 : 0);
}

void fill_eeprom_contents(carvera::sim::v1::EepromContents& output, const EEPROM_data& data) {
  output.set_tool_length_offset(data.TLO);
  output.set_reference_machine_z(data.REFMZ);
  output.set_tool_machine_z(data.TOOLMZ);
  output.set_reserved(data.reserve);
  output.set_active_tool(data.TOOL);
  output.set_tool_not_calibrated(data.tool_not_calibrated);
  output.set_current_wcs(data.current_wcs);

  for (int index = 0; index < persistent_variable_count; ++index) {
    auto* variable = output.add_persistent_variables();
    variable->set_number(persistent_variable_base + index);
    variable->set_value(data.perm_vars[index]);
  }

  for (int index = 0; index < work_coordinate_system_count; ++index) {
    auto* work_coordinate_system = output.add_work_coordinate_systems();
    work_coordinate_system->set_number(work_coordinate_system_base + index);
    work_coordinate_system->set_x(data.WCScoord[index][0]);
    work_coordinate_system->set_y(data.WCScoord[index][1]);
    work_coordinate_system->set_z(data.WCScoord[index][2]);
    work_coordinate_system->set_a(data.WCScoord[index][3]);
    work_coordinate_system->set_rotation(data.WCSrotation[index]);
  }
}

bool apply_eeprom_contents(EEPROM_data& data, const carvera::sim::v1::EepromContents& contents) {
  if (contents.persistent_variables_size() != persistent_variable_count ||
      contents.work_coordinate_systems_size() != work_coordinate_system_count) {
    return false;
  }

  data.TLO = static_cast<float>(contents.tool_length_offset());
  data.REFMZ = static_cast<float>(contents.reference_machine_z());
  data.TOOLMZ = static_cast<float>(contents.tool_machine_z());
  data.reserve = static_cast<float>(contents.reserved());
  data.TOOL = contents.active_tool();
  data.tool_not_calibrated = contents.tool_not_calibrated();
  data.current_wcs = contents.current_wcs();

  std::array<bool, persistent_variable_count> seen_variables{};
  for (const auto& variable : contents.persistent_variables()) {
    const int index = static_cast<int>(variable.number()) - persistent_variable_base;
    if (index < 0 || index >= persistent_variable_count || seen_variables[static_cast<std::size_t>(index)]) {
      return false;
    }
    seen_variables[static_cast<std::size_t>(index)] = true;
    data.perm_vars[index] = static_cast<float>(variable.value());
  }

  std::array<bool, work_coordinate_system_count> seen_work_coordinate_systems{};
  for (const auto& work_coordinate_system : contents.work_coordinate_systems()) {
    const int index = static_cast<int>(work_coordinate_system.number()) - work_coordinate_system_base;
    if (index < 0 || index >= work_coordinate_system_count ||
        seen_work_coordinate_systems[static_cast<std::size_t>(index)]) {
      return false;
    }
    seen_work_coordinate_systems[static_cast<std::size_t>(index)] = true;
    data.WCScoord[index][0] = static_cast<float>(work_coordinate_system.x());
    data.WCScoord[index][1] = static_cast<float>(work_coordinate_system.y());
    data.WCScoord[index][2] = static_cast<float>(work_coordinate_system.z());
    data.WCScoord[index][3] = static_cast<float>(work_coordinate_system.a());
    data.WCSrotation[index] = static_cast<float>(work_coordinate_system.rotation());
  }
  return true;
}

}  // namespace

std::optional<ApiService::Response> ApiService::handle_harness_command(const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;
  auto& eeprom = persistent_state_.eeprom();

  switch (request.command_case()) {
    case Request::kSetGpioInput: {
      const auto& command = request.set_gpio_input();
      if (!api::valid_pin(command.pin())) {
        return error(request.id(), "invalid GPIO pin");
      }
      machine_.set_gpio_input(api::pin_address(command.pin()), command.high());
      return ok(request.id());
    }
    case Request::kGetGpioLevel: {
      const auto& command = request.get_gpio_level();
      if (!api::valid_pin(command.pin())) {
        return error(request.id(), "invalid GPIO pin");
      }
      auto response = ok(request.id());
      auto* level = response.mutable_gpio_level();
      level->mutable_pin()->CopyFrom(command.pin());
      level->set_high(machine_.gpio_level(api::pin_address(command.pin())));
      return response;
    }
    case Request::kAttachStepDirAxis: {
      const auto& command = request.attach_step_dir_axis();
      if (!api::valid_pin(command.step_pin()) || !api::valid_pin(command.direction_pin())) {
        return error(request.id(), "invalid step/dir axis pin");
      }
      const auto axis = machine_.add_step_dir_axis(
          api::pin_address(command.step_pin()), api::pin_address(command.direction_pin()), command.invert_direction());
      auto response = ok(request.id());
      response.mutable_attached_axis()->set_axis(static_cast<std::uint32_t>(axis));
      return response;
    }
    case Request::kGetAxisPosition: {
      try {
        auto response = ok(request.id());
        auto* position = response.mutable_axis_position();
        position->set_axis(request.get_axis_position().axis());
        position->set_steps(machine_.axis_position_steps(request.get_axis_position().axis()));
        return response;
      } catch (const std::exception&) {
        return error(request.id(), "invalid axis");
      }
    }
    case Request::kGetPwmOutput: {
      const auto& command = request.get_pwm_output();
      if (!api::valid_pin(command.pin())) {
        return error(request.id(), "invalid PWM pin");
      }
      const auto state = machine_.pwm_output(api::pin_address(command.pin()));
      auto response = ok(request.id());
      auto* output = response.mutable_pwm_output();
      output->mutable_pin()->CopyFrom(command.pin());
      output->set_configured(state.configured);
      output->set_duty(state.duty);
      output->set_period_us(state.period_us);
      return response;
    }
    case Request::kTriggerInterruptRise: {
      const auto& command = request.trigger_interrupt_rise();
      if (!api::valid_pin(command.pin())) {
        return error(request.id(), "invalid interrupt pin");
      }
      machine_.trigger_interrupt_rise(api::pin_address(command.pin()));
      return ok(request.id());
    }
    case Request::kSetAdcInput: {
      const auto& command = request.set_adc_input();
      if (command.channel() > 7) {
        return error(request.id(), "invalid ADC channel");
      }
      if (command.raw() > 4095) {
        return error(request.id(), "ADC raw value must be 0..4095");
      }
      machine_.set_adc_channel_raw(static_cast<std::uint8_t>(command.channel()),
                                   static_cast<std::uint16_t>(command.raw()));
      return ok(request.id());
    }
    case Request::kGetAdcInput: {
      const auto& command = request.get_adc_input();
      if (command.channel() > 7) {
        return error(request.id(), "invalid ADC channel");
      }
      auto response = ok(request.id());
      auto* input = response.mutable_adc_input();
      input->set_channel(command.channel());
      input->set_raw(machine_.adc_channel_raw(static_cast<std::uint8_t>(command.channel())));
      return response;
    }
    case Request::kGetEepromBytes: {
      const auto& command = request.get_eeprom_bytes();
      const std::uint64_t end = static_cast<std::uint64_t>(command.offset()) + command.length();
      if (end > i2c_eeprom::size) {
        return error(request.id(), "EEPROM range is out of bounds");
      }
      std::string bytes;
      bytes.reserve(command.length());
      for (std::uint32_t offset = 0; offset < command.length(); ++offset) {
        bytes.push_back(static_cast<char>(eeprom.peek(static_cast<std::uint16_t>(command.offset() + offset))));
      }
      auto response = ok(request.id());
      auto* output = response.mutable_eeprom_bytes();
      output->set_offset(command.offset());
      output->set_data(bytes);
      output->set_total_size(static_cast<std::uint32_t>(i2c_eeprom::size));
      return response;
    }
    case Request::kSetEepromBytes: {
      const auto& command = request.set_eeprom_bytes();
      const std::uint64_t end = static_cast<std::uint64_t>(command.offset()) + command.data().size();
      if (end > i2c_eeprom::size) {
        return error(request.id(), "EEPROM range is out of bounds");
      }
      for (std::size_t offset = 0; offset < command.data().size(); ++offset) {
        eeprom.poke(static_cast<std::uint16_t>(command.offset() + offset),
                    static_cast<std::uint8_t>(command.data()[offset]));
      }
      return ok(request.id());
    }
    case Request::kGetEepromContents: {
      auto response = ok(request.id());
      fill_eeprom_contents(*response.mutable_eeprom_contents(), read_persistent_data(eeprom));
      return response;
    }
    case Request::kSetEepromContents: {
      auto data = read_persistent_data(eeprom);
      if (!apply_eeprom_contents(data, request.set_eeprom_contents().contents())) {
        return error(request.id(), "invalid EEPROM contents");
      }
      write_persistent_data(eeprom, data);
      return ok(request.id());
    }
    default:
      return std::nullopt;
  }
}

std::optional<ApiService::Response> ApiService::handle_cooperative_harness_command(
    const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kSetGpioInput:
    case Request::kGetGpioLevel:
    case Request::kGetAxisPosition:
    case Request::kGetPwmOutput:
    case Request::kSetAdcInput:
    case Request::kGetAdcInput:
    case Request::kGetEepromBytes:
    case Request::kSetEepromBytes:
    case Request::kGetEepromContents:
    case Request::kSetEepromContents:
      return handle_harness_command(request);
    default:
      return std::nullopt;
  }
}

}  // namespace sim
