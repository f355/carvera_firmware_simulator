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

#include <string>

#include "sim/i2c_eeprom.hpp"
#include "support/api_service_harness.hpp"
#include "support/assertions.hpp"

int main() {
  using sim::test::require;

  sim::test::ApiHarness api;

  auto response = api.request([](auto& request) {
    request.mutable_set_eeprom_bytes()->set_offset(0x24);
    request.mutable_set_eeprom_bytes()->set_data(std::string("\x12\x34\x56", 3));
  });
  require(response.ok(), "set_eeprom_bytes should write a simulator EEPROM byte range");

  response = api.request([](auto& request) {
    request.mutable_get_eeprom_bytes()->set_offset(0x20);
    request.mutable_get_eeprom_bytes()->set_length(8);
  });
  require(response.ok(), "get_eeprom_bytes should read a simulator EEPROM byte range");
  require(response.eeprom_bytes().offset() == 0x20, "EEPROM response should echo the requested offset");
  require(response.eeprom_bytes().total_size() == sim::i2c_eeprom::size, "EEPROM response should expose total size");
  require(response.eeprom_bytes().data().size() == 8, "EEPROM response should preserve requested byte count");
  require(static_cast<unsigned char>(response.eeprom_bytes().data()[4]) == 0x12 &&
              static_cast<unsigned char>(response.eeprom_bytes().data()[5]) == 0x34 &&
              static_cast<unsigned char>(response.eeprom_bytes().data()[6]) == 0x56,
          "EEPROM response should include bytes written through the API");

  response = api.request([](auto& request) {
    request.mutable_get_eeprom_bytes()->set_offset(sim::i2c_eeprom::size - 2);
    request.mutable_get_eeprom_bytes()->set_length(3);
  });
  require(!response.ok(), "get_eeprom_bytes should reject ranges past EEPROM end");

  response = api.request([](auto& request) { request.mutable_get_eeprom_fields(); });
  require(response.ok(), "get_eeprom_fields should read named firmware EEPROM fields");
  require(response.eeprom_fields().fields_size() > 40, "get_eeprom_fields should expose persistent firmware fields");

  response = api.request([](auto& request) {
    auto* tlo_field = request.mutable_set_eeprom_fields()->add_fields();
    tlo_field->set_name("TLO");
    tlo_field->set_type(sim::test::pb::EEPROM_FIELD_TYPE_FLOAT);
    tlo_field->set_number(12.5);
    auto* tool_field = request.mutable_set_eeprom_fields()->add_fields();
    tool_field->set_name("TOOL");
    tool_field->set_type(sim::test::pb::EEPROM_FIELD_TYPE_INT);
    tool_field->set_integer(4);
    auto* calibrated_field = request.mutable_set_eeprom_fields()->add_fields();
    calibrated_field->set_name("tool_not_calibrated");
    calibrated_field->set_type(sim::test::pb::EEPROM_FIELD_TYPE_BOOL);
    calibrated_field->set_boolean(true);
  });
  require(response.ok(), "set_eeprom_fields should update named firmware EEPROM fields");

  response = api.request([](auto& request) { request.mutable_get_eeprom_fields(); });
  require(response.ok(), "get_eeprom_fields should read back updated fields");
  bool saw_tlo = false;
  bool saw_tool = false;
  bool saw_flag = false;
  for (const auto& field : response.eeprom_fields().fields()) {
    saw_tlo = saw_tlo || (field.name() == "TLO" && field.number() == 12.5);
    saw_tool = saw_tool || (field.name() == "TOOL" && field.integer() == 4);
    saw_flag = saw_flag || (field.name() == "tool_not_calibrated" && field.boolean());
  }
  require(saw_tlo && saw_tool && saw_flag, "get_eeprom_fields should reflect named field updates");

  return 0;
}
