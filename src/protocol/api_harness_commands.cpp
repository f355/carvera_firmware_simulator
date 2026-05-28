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

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "libs/Kernel.h"
#include "sim/api_conversions.hpp"
#include "sim/i2c_eeprom.hpp"

namespace sim {
namespace {

constexpr std::uint16_t eeprom_data_offset = 32;

EEPROM_data read_persistent_data() {
  EEPROM_data data{};
  auto* output = reinterpret_cast<std::uint8_t*>(&data);
  for (std::size_t offset = 0; offset < sizeof(EEPROM_data); ++offset) {
    output[offset] = i2c_eeprom::read(static_cast<std::uint16_t>(eeprom_data_offset + offset));
  }
  return data;
}

void write_persistent_data(const EEPROM_data& data) {
  const auto* input = reinterpret_cast<const std::uint8_t*>(&data);
  for (std::size_t offset = 0; offset < sizeof(EEPROM_data); ++offset) {
    i2c_eeprom::write(static_cast<std::uint16_t>(eeprom_data_offset + offset), input[offset]);
  }
}

void add_float_field(carvera::sim::v1::EepromFields& output, const char* name, float value) {
  auto* field = output.add_fields();
  field->set_name(name);
  field->set_type(carvera::sim::v1::EEPROM_FIELD_TYPE_FLOAT);
  field->set_number(value);
}

void add_int_field(carvera::sim::v1::EepromFields& output, const char* name, int value) {
  auto* field = output.add_fields();
  field->set_name(name);
  field->set_type(carvera::sim::v1::EEPROM_FIELD_TYPE_INT);
  field->set_integer(value);
}

void add_bool_field(carvera::sim::v1::EepromFields& output, const char* name, bool value) {
  auto* field = output.add_fields();
  field->set_name(name);
  field->set_type(carvera::sim::v1::EEPROM_FIELD_TYPE_BOOL);
  field->set_boolean(value);
}

void fill_eeprom_fields(carvera::sim::v1::EepromFields& output, const EEPROM_data& data) {
  add_float_field(output, "TLO", data.TLO);
  add_float_field(output, "REFMZ", data.REFMZ);
  add_float_field(output, "TOOLMZ", data.TOOLMZ);
  add_float_field(output, "reserve", data.reserve);
  add_int_field(output, "TOOL", data.TOOL);
  add_bool_field(output, "tool_not_calibrated", data.tool_not_calibrated);
  add_int_field(output, "current_wcs", data.current_wcs);

  char name[32] = {};
  for (int index = 0; index < 20; ++index) {
    std::snprintf(name, sizeof(name), "perm_vars[%d]", 501 + index);
    add_float_field(output, name, data.perm_vars[index]);
  }

  constexpr const char* axes[] = {"X", "Y", "Z", "A"};
  for (int wcs = 0; wcs < 6; ++wcs) {
    for (int axis = 0; axis < 4; ++axis) {
      std::snprintf(name, sizeof(name), "G5%d.%s", 4 + wcs, axes[axis]);
      add_float_field(output, name, data.WCScoord[wcs][axis]);
    }
    std::snprintf(name, sizeof(name), "G5%d.rotation", 4 + wcs);
    add_float_field(output, name, data.WCSrotation[wcs]);
  }
}

bool apply_eeprom_field(EEPROM_data& data, const carvera::sim::v1::EepromField& field) {
  const auto& name = field.name();
  if (name == "TLO") {
    data.TLO = static_cast<float>(field.number());
  } else if (name == "REFMZ") {
    data.REFMZ = static_cast<float>(field.number());
  } else if (name == "TOOLMZ") {
    data.TOOLMZ = static_cast<float>(field.number());
  } else if (name == "reserve") {
    data.reserve = static_cast<float>(field.number());
  } else if (name == "TOOL") {
    data.TOOL = static_cast<int>(field.integer());
  } else if (name == "tool_not_calibrated") {
    data.tool_not_calibrated = field.boolean();
  } else if (name == "current_wcs") {
    data.current_wcs = static_cast<int>(field.integer());
  } else {
    int index = 0;
    if (std::sscanf(name.c_str(), "perm_vars[%d]", &index) == 1 && index >= 501 && index <= 520) {
      data.perm_vars[index - 501] = static_cast<float>(field.number());
      return true;
    }

    int wcs = 0;
    char suffix[16] = {};
    if (std::sscanf(name.c_str(), "G5%d.%15s", &wcs, suffix) == 2 && wcs >= 4 && wcs <= 9) {
      const int wcs_index = wcs - 4;
      if (std::strcmp(suffix, "X") == 0) {
        data.WCScoord[wcs_index][0] = static_cast<float>(field.number());
      } else if (std::strcmp(suffix, "Y") == 0) {
        data.WCScoord[wcs_index][1] = static_cast<float>(field.number());
      } else if (std::strcmp(suffix, "Z") == 0) {
        data.WCScoord[wcs_index][2] = static_cast<float>(field.number());
      } else if (std::strcmp(suffix, "A") == 0) {
        data.WCScoord[wcs_index][3] = static_cast<float>(field.number());
      } else if (std::strcmp(suffix, "rotation") == 0) {
        data.WCSrotation[wcs_index] = static_cast<float>(field.number());
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<ApiService::Response> ApiService::handle_harness_command(const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kSetGpioInput: {
      const auto& command = request.set_gpio_input();
      if (!api::valid_pin(command.pin())) {
        return error(request.id(), "invalid GPIO pin");
      }
      simulator_.set_gpio_input(api::pin_address(command.pin()), command.high());
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
      level->set_high(simulator_.gpio_level(api::pin_address(command.pin())));
      return response;
    }
    case Request::kAttachStepDirAxis: {
      const auto& command = request.attach_step_dir_axis();
      if (!api::valid_pin(command.step_pin()) || !api::valid_pin(command.direction_pin())) {
        return error(request.id(), "invalid step/dir axis pin");
      }
      const auto axis = simulator_.add_step_dir_axis(
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
        position->set_steps(simulator_.axis_position_steps(request.get_axis_position().axis()));
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
      const auto state = simulator_.pwm_output(api::pin_address(command.pin()));
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
      simulator_.trigger_interrupt_rise(api::pin_address(command.pin()));
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
      simulator_.set_adc_channel_raw(static_cast<std::uint8_t>(command.channel()),
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
      input->set_raw(simulator_.adc_channel_raw(static_cast<std::uint8_t>(command.channel())));
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
        bytes.push_back(static_cast<char>(i2c_eeprom::read(static_cast<std::uint16_t>(command.offset() + offset))));
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
        i2c_eeprom::write(static_cast<std::uint16_t>(command.offset() + offset),
                          static_cast<std::uint8_t>(command.data()[offset]));
      }
      return ok(request.id());
    }
    case Request::kGetEepromFields: {
      auto response = ok(request.id());
      fill_eeprom_fields(*response.mutable_eeprom_fields(), read_persistent_data());
      return response;
    }
    case Request::kSetEepromFields: {
      auto data = read_persistent_data();
      for (const auto& field : request.set_eeprom_fields().fields()) {
        if (!apply_eeprom_field(data, field)) {
          return error(request.id(), "unsupported EEPROM field");
        }
      }
      write_persistent_data(data);
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
    case Request::kGetEepromFields:
    case Request::kSetEepromFields:
      return handle_harness_command(request);
    default:
      return std::nullopt;
  }
}

}  // namespace sim
