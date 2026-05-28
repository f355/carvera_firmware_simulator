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

#include "carvera_sim.pb.h"
#include "sim/api_conversions.hpp"
#include "support/assertions.hpp"

int main() {
  using sim::test::require;
  namespace pb = carvera::sim::v1;

  pb::Box proto_box;
  proto_box.set_min_x(-1.0);
  proto_box.set_min_y(-2.0);
  proto_box.set_min_z(-3.0);
  proto_box.set_max_x(1.0);
  proto_box.set_max_y(2.0);
  proto_box.set_max_z(3.0);
  const auto box = sim::api::box_from_proto(proto_box);
  require(box.min_x == -1.0 && box.max_z == 3.0, "box conversion should preserve coordinates");

  require(sim::api::axis_index(pb::AXIS_X).value_or(99) == 0, "X axis should map to physical axis 0");
  require(sim::api::axis_index(pb::AXIS_B).value_or(99) == 4, "B axis should map to physical axis 4");
  require(!sim::api::axis_index(pb::AXIS_UNSPECIFIED).has_value(), "unspecified axis should not map");

  require(sim::api::limit_switch_side(pb::LIMIT_SWITCH_SIDE_MIN).value() == sim::LimitSwitchSide::Min,
          "min limit side should map");
  require(!sim::api::limit_switch_side(pb::LIMIT_SWITCH_SIDE_UNSPECIFIED).has_value(),
          "unspecified limit side should not map");

  require(sim::api::temperature_sensor(pb::TEMPERATURE_SENSOR_POWER).value() == sim::TemperatureSensor::Power,
          "power temperature sensor should map");
  require(sim::api::machine_switch(pb::SWITCH_NAME_BEEP).value() == sim::MachineSwitch::Beep, "beep switch should map");

  pb::PinAddress pin;
  pin.set_port(1);
  pin.set_pin(31);
  require(sim::api::valid_pin(pin), "valid LPC pin should pass");
  const auto native_pin = sim::api::pin_address(pin);
  require(native_pin.port == 1 && native_pin.pin == 31, "pin conversion should preserve port and pin");
  pin.set_port(5);
  require(!sim::api::valid_pin(pin), "invalid LPC port should fail");

  require(sim::api::proto_machine_model(sim::MachineModel::CarveraAirCA1) == pb::MACHINE_MODEL_CARVERA_AIR_CA1,
          "CA1 model should map to protobuf");
  require(sim::api::machine_model(pb::MACHINE_MODEL_CARVERA_C1) == sim::MachineModel::CarveraC1,
          "C1 protobuf model should map to simulator model");
  require(sim::api::machine_model(pb::MACHINE_MODEL_UNSPECIFIED) == sim::MachineModel{},
          "unspecified protobuf model should map to empty simulator model");

  require(std::string(sim::api::axis_letter(pb::AXIS_A)) == "A", "A axis letter should map");
  require(sim::api::axis_letter(pb::AXIS_UNSPECIFIED) == nullptr, "unspecified axis should not have a letter");

  pb::Jog jog;
  jog.set_feed_rate(600.0);
  auto* x = jog.add_delta();
  x->set_axis(pb::AXIS_X);
  x->set_distance(1.25);
  auto* y = jog.add_delta();
  y->set_axis(pb::AXIS_Y);
  y->set_distance(-2.0);
  require(sim::api::jog_command(jog) == "G0 X1.25 Y-2 F600\n", "jog command should format valid deltas");
  jog.mutable_delta(1)->set_distance(0.0);
  require(sim::api::jog_command(jog).empty(), "zero-distance jog delta should be rejected");

  return 0;
}
