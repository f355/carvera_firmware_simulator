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

#include "sim/api_conversions.hpp"

#include <iomanip>
#include <sstream>

namespace sim::api {

Box box_from_proto(const carvera::sim::v1::Box& box) {
  return Box{
      box.min_x(), box.min_y(), box.min_z(), box.max_x(), box.max_y(), box.max_z(),
  };
}

std::optional<std::size_t> axis_index(carvera::sim::v1::Axis axis) {
  switch (axis) {
    case carvera::sim::v1::AXIS_X:
      return 0;
    case carvera::sim::v1::AXIS_Y:
      return 1;
    case carvera::sim::v1::AXIS_Z:
      return 2;
    case carvera::sim::v1::AXIS_A:
      return 3;
    case carvera::sim::v1::AXIS_B:
      return 4;
    default:
      return std::nullopt;
  }
}

std::optional<LimitSwitchSide> limit_switch_side(carvera::sim::v1::LimitSwitchSide side) {
  switch (side) {
    case carvera::sim::v1::LIMIT_SWITCH_SIDE_MIN:
      return LimitSwitchSide::Min;
    case carvera::sim::v1::LIMIT_SWITCH_SIDE_MAX:
      return LimitSwitchSide::Max;
    default:
      return std::nullopt;
  }
}

std::optional<TemperatureSensor> temperature_sensor(carvera::sim::v1::TemperatureSensor sensor) {
  switch (sensor) {
    case carvera::sim::v1::TEMPERATURE_SENSOR_SPINDLE:
      return TemperatureSensor::Spindle;
    case carvera::sim::v1::TEMPERATURE_SENSOR_POWER:
      return TemperatureSensor::Power;
    default:
      return std::nullopt;
  }
}

std::optional<MachineSwitch> machine_switch(carvera::sim::v1::SwitchName name) {
  switch (name) {
    case carvera::sim::v1::SWITCH_NAME_LIGHT:
      return MachineSwitch::Light;
    case carvera::sim::v1::SWITCH_NAME_VACUUM:
      return MachineSwitch::Vacuum;
    case carvera::sim::v1::SWITCH_NAME_SPINDLE_FAN:
      return MachineSwitch::SpindleFan;
    case carvera::sim::v1::SWITCH_NAME_POWER_FAN:
      return MachineSwitch::PowerFan;
    case carvera::sim::v1::SWITCH_NAME_TOOL_SENSOR:
      return MachineSwitch::ToolSensor;
    case carvera::sim::v1::SWITCH_NAME_PROBE_CHARGER:
      return MachineSwitch::ProbeCharger;
    case carvera::sim::v1::SWITCH_NAME_AIR:
      return MachineSwitch::Air;
    case carvera::sim::v1::SWITCH_NAME_BEEP:
      return MachineSwitch::Beep;
    default:
      return std::nullopt;
  }
}

ToolKind tool_kind(carvera::sim::v1::ToolKind kind) {
  switch (kind) {
    case carvera::sim::v1::TOOL_KIND_CUTTING_TOOL:
      return ToolKind::CuttingTool;
    case carvera::sim::v1::TOOL_KIND_STOCK_Z_PROBE:
      return ToolKind::StockZProbe;
    case carvera::sim::v1::TOOL_KIND_THREE_AXIS_PROBE:
      return ToolKind::ThreeAxisProbe;
    case carvera::sim::v1::TOOL_KIND_UNSPECIFIED:
    default:
      return ToolKind::Unspecified;
  }
}

carvera::sim::v1::ToolKind proto_tool_kind(ToolKind kind) {
  switch (kind) {
    case ToolKind::CuttingTool:
      return carvera::sim::v1::TOOL_KIND_CUTTING_TOOL;
    case ToolKind::StockZProbe:
      return carvera::sim::v1::TOOL_KIND_STOCK_Z_PROBE;
    case ToolKind::ThreeAxisProbe:
      return carvera::sim::v1::TOOL_KIND_THREE_AXIS_PROBE;
    case ToolKind::Unspecified:
    default:
      return carvera::sim::v1::TOOL_KIND_UNSPECIFIED;
  }
}

bool valid_pin(const carvera::sim::v1::PinAddress& pin) { return pin.port() <= 4 && pin.pin() <= 31; }

PinAddress pin_address(const carvera::sim::v1::PinAddress& pin) {
  return PinAddress{
      static_cast<std::uint8_t>(pin.port()),
      static_cast<std::uint8_t>(pin.pin()),
  };
}

carvera::sim::v1::TimeMode time_mode(bool realtime) {
  return realtime ? carvera::sim::v1::TIME_MODE_REALTIME : carvera::sim::v1::TIME_MODE_MANUAL;
}

carvera::sim::v1::MachineModel proto_machine_model(MachineModel model) {
  switch (model) {
    case MachineModel::CarveraC1:
      return carvera::sim::v1::MACHINE_MODEL_CARVERA_C1;
    case MachineModel::CarveraAirCA1:
      return carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1;
  }
  return carvera::sim::v1::MACHINE_MODEL_UNSPECIFIED;
}

MachineModel machine_model(carvera::sim::v1::MachineModel model) {
  switch (model) {
    case carvera::sim::v1::MACHINE_MODEL_CARVERA_C1:
      return MachineModel::CarveraC1;
    case carvera::sim::v1::MACHINE_MODEL_CARVERA_AIR_CA1:
      return MachineModel::CarveraAirCA1;
    default:
      return {};
  }
}

const char* axis_letter(carvera::sim::v1::Axis axis) {
  switch (axis) {
    case carvera::sim::v1::AXIS_X:
      return "X";
    case carvera::sim::v1::AXIS_Y:
      return "Y";
    case carvera::sim::v1::AXIS_Z:
      return "Z";
    case carvera::sim::v1::AXIS_A:
      return "A";
    case carvera::sim::v1::AXIS_B:
      return "B";
    default:
      return nullptr;
  }
}

std::string jog_command(const carvera::sim::v1::Jog& jog) {
  if (jog.delta().empty() || jog.feed_rate() <= 0.0) {
    return "";
  }

  std::ostringstream command;
  command << std::setprecision(9) << "G0";
  for (const auto& delta : jog.delta()) {
    const char* axis = axis_letter(delta.axis());
    if (axis == nullptr || delta.distance() == 0.0) {
      return "";
    }
    command << ' ' << axis << delta.distance();
  }
  command << " F" << jog.feed_rate() << '\n';
  return command.str();
}

}  // namespace sim::api
