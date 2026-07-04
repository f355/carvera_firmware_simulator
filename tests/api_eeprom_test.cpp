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

  response = api.request([](auto& request) { request.mutable_get_eeprom_contents(); });
  require(response.ok(), "get_eeprom_contents should read structured firmware EEPROM contents");
  require(response.has_eeprom_contents(), "get_eeprom_contents should return structured contents");
  require(response.eeprom_contents().persistent_variables_size() == 20,
          "EEPROM contents should expose all persistent variables");
  require(response.eeprom_contents().persistent_variables(0).number() == 501,
          "persistent variables should carry their firmware number");
  require(response.eeprom_contents().work_coordinate_systems_size() == 6,
          "EEPROM contents should expose G54 through G59");
  require(response.eeprom_contents().work_coordinate_systems(0).number() == 54,
          "work coordinate systems should carry their G-code number");

  std::string serialized_contents;
  require(response.SerializeToString(&serialized_contents), "EEPROM contents response should serialize");
  sim::test::pb::Response parsed_contents;
  require(parsed_contents.ParseFromString(serialized_contents), "serialized EEPROM contents response should parse");
  require(parsed_contents.eeprom_contents().persistent_variables_size() == 20,
          "serialized EEPROM contents should preserve persistent variables");

  auto updated_contents = response.eeprom_contents();
  updated_contents.set_tool_length_offset(12.5);
  updated_contents.set_active_tool(4);
  updated_contents.set_tool_not_calibrated(false);
  response = api.request([&updated_contents](auto& request) {
    request.mutable_set_eeprom_contents()->mutable_contents()->CopyFrom(updated_contents);
  });
  require(response.ok(), "set_eeprom_contents should update structured firmware EEPROM contents");

  response = api.request([](auto& request) { request.mutable_get_eeprom_contents(); });
  require(response.ok(), "get_eeprom_contents should read back updated contents");
  require(response.eeprom_contents().tool_length_offset() == 12.5,
          "structured EEPROM contents should preserve tool length offset");
  require(response.eeprom_contents().active_tool() == 4,
          "structured EEPROM contents should preserve the active tool");
  require(!response.eeprom_contents().tool_not_calibrated(),
          "structured EEPROM contents should preserve calibration state");

  return 0;
}
