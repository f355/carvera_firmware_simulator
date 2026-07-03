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

#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

#include "sim/api_conversions.hpp"
#include "sim/logging.hpp"
#include "sim/physical_scene.hpp"
#include "sim/runtime_atc_config.hpp"
#include "sim/simulator_context.hpp"

namespace sim {

namespace {

constexpr carvera::sim::v1::Axis kSnapshotAxes[] = {
    carvera::sim::v1::AXIS_X, carvera::sim::v1::AXIS_Y, carvera::sim::v1::AXIS_Z,
    carvera::sim::v1::AXIS_A, carvera::sim::v1::AXIS_B,
};

constexpr carvera::sim::v1::SwitchName kSnapshotSwitches[] = {
    carvera::sim::v1::SWITCH_NAME_LIGHT,       carvera::sim::v1::SWITCH_NAME_VACUUM,
    carvera::sim::v1::SWITCH_NAME_SPINDLE_FAN, carvera::sim::v1::SWITCH_NAME_POWER_FAN,
    carvera::sim::v1::SWITCH_NAME_TOOL_SENSOR, carvera::sim::v1::SWITCH_NAME_PROBE_CHARGER,
    carvera::sim::v1::SWITCH_NAME_AIR,         carvera::sim::v1::SWITCH_NAME_BEEP,
};

constexpr PinAddress kSnapshotPwmPins[] = {
    {2, 5}, {2, 4}, {2, 3}, {2, 1}, {2, 3}, {2, 2},
};

const char* bool_text(bool value) { return value ? "on" : "off"; }

const char* switch_name(carvera::sim::v1::SwitchName name) {
  switch (name) {
    case carvera::sim::v1::SWITCH_NAME_LIGHT:
      return "light";
    case carvera::sim::v1::SWITCH_NAME_VACUUM:
      return "vacuum";
    case carvera::sim::v1::SWITCH_NAME_SPINDLE_FAN:
      return "spindle_fan";
    case carvera::sim::v1::SWITCH_NAME_POWER_FAN:
      return "power_fan";
    case carvera::sim::v1::SWITCH_NAME_TOOL_SENSOR:
      return "tool_sensor";
    case carvera::sim::v1::SWITCH_NAME_PROBE_CHARGER:
      return "probe_charger";
    case carvera::sim::v1::SWITCH_NAME_AIR:
      return "air";
    case carvera::sim::v1::SWITCH_NAME_BEEP:
      return "beep";
    default:
      return "unknown";
  }
}

const char* temperature_sensor_name(carvera::sim::v1::TemperatureSensor sensor) {
  switch (sensor) {
    case carvera::sim::v1::TEMPERATURE_SENSOR_SPINDLE:
      return "spindle";
    case carvera::sim::v1::TEMPERATURE_SENSOR_POWER:
      return "power";
    default:
      return "unknown";
  }
}

std::string box_summary(const carvera::sim::v1::Box& box) {
  std::ostringstream message;
  message << std::fixed << std::setprecision(3) << '[' << box.min_x() << ',' << box.min_y() << ',' << box.min_z()
          << "]..[" << box.max_x() << ',' << box.max_y() << ',' << box.max_z() << ']';
  return message.str();
}

void fill_front_panel(carvera::sim::v1::FrontPanelState& state, const FrontPanelState& panel) {
  state.set_main_button_pressed(panel.main_button_pressed);
  state.set_e_stop_pressed(panel.e_stop_pressed);
  state.mutable_power_rails()->set_v12(panel.power_rails.v12);
  state.mutable_power_rails()->set_v24(panel.power_rails.v24);
  state.set_direct_rgb_available(panel.direct_rgb.available);
  state.mutable_direct_rgb()->set_r(panel.direct_rgb.red ? 255 : 0);
  state.mutable_direct_rgb()->set_g(panel.direct_rgb.green ? 255 : 0);
  state.mutable_direct_rgb()->set_b(panel.direct_rgb.blue ? 255 : 0);
  state.set_led_strip_available(panel.led_strip.available);
  for (const auto& segment : panel.led_strip.segments) {
    auto* output = state.add_led_strip();
    output->set_r(segment.red);
    output->set_g(segment.green);
    output->set_b(segment.blue);
  }
}

void fill_pin(carvera::sim::v1::PinAddress& target, PinAddress source) {
  target.set_port(source.port);
  target.set_pin(source.pin);
}

void apply_tool_setter_box(PhysicalScene& scene, FirmwareRuntime& firmware,
                           const carvera::sim::v1::SetToolSetterBox& command) {
  if (command.enabled()) {
    firmware.set_tool_setter_box(api::box_from_proto(command.bounds()));
    logging::event("physical", "tool setter box " + box_summary(command.bounds()));
  } else {
    scene.clear_tool_setter_box();
    logging::event("physical", "tool setter box disabled");
  }
}

void apply_stock_box(PhysicalScene& scene, FirmwareRuntime& firmware,
                     const carvera::sim::v1::SetStockBox& command) {
  if (command.enabled()) {
    firmware.set_stock_box(api::box_from_proto(command.bounds()));
    logging::event("physical", "stock box " + box_summary(command.bounds()));
  } else {
    scene.clear_stock_box();
    logging::event("physical", "stock box disabled");
  }
}

std::optional<const char*> apply_atc_pocket_tools(PhysicalScene& scene, FirmwareRuntime& firmware,
                                                  const carvera::sim::v1::SetAtcPocketTools& command) {
  if (command.replace()) {
    scene.clear_atc_pocket_tools();
  }
  for (const auto& tool : command.tools()) {
    if (tool.length_mm() < 0.0) {
      return "ATC tool length must be non-negative";
    }
    if (tool.probe_tip_diameter_mm() < 0.0) {
      return "ATC probe tip diameter must be non-negative";
    }
    scene.set_atc_pocket_tool(static_cast<int>(tool.pocket()), tool.tool(), tool.occupied(), tool.length_mm(),
                              api::tool_kind(tool.kind()), tool.probe_tip_diameter_mm());
  }
  if (firmware.booted()) {
    runtime_atc::configure_physical_scene(firmware.boot(), scene, true);
  }
  std::ostringstream message;
  message << "ATC pocket table updated tools=" << command.tools_size();
  if (command.replace()) {
    message << " replace";
  }
  logging::event("physical", message.str());
  return std::nullopt;
}

std::optional<const char*> apply_spindle_tool(PhysicalScene& scene,
                                              const carvera::sim::v1::SetSpindleTool& command) {
  if (command.length_mm() < 0.0) {
    return "spindle tool length must be non-negative";
  }
  if (command.probe_tip_diameter_mm() < 0.0) {
    return "spindle probe tip diameter must be non-negative";
  }
  scene.set_spindle_tool(command.tool(), command.length_mm(), command.installed(), api::tool_kind(command.kind()),
                         command.probe_tip_diameter_mm());
  std::ostringstream message;
  message << std::fixed << std::setprecision(3);
  if (command.installed()) {
    message << "spindle tool T" << command.tool() << " length=" << command.length_mm() << "mm";
  } else {
    message << "spindle tool removed";
  }
  logging::event("physical", message.str());
  return std::nullopt;
}

std::optional<const char*> apply_temperature(FirmwareRuntime& firmware,
                                             const carvera::sim::v1::SetTemperature& command) {
  const auto sensor = api::temperature_sensor(command.sensor());
  if (!sensor.has_value()) {
    return "unsupported temperature sensor";
  }
  if (!std::isfinite(command.celsius()) || command.celsius() <= -273.15) {
    return "temperature must be finite and above absolute zero";
  }
  firmware.set_temperature(*sensor, command.celsius());
  std::ostringstream message;
  message << std::fixed << std::setprecision(1) << temperature_sensor_name(command.sensor()) << " temperature "
          << command.celsius() << "C";
  logging::event("physical", message.str());
  return std::nullopt;
}

}  // namespace

std::optional<ApiService::Response> ApiService::handle_physical_command(const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kSetProbeInputs:
      firmware_.set_probe_inputs(request.set_probe_inputs().probe(), request.set_probe_inputs().tool_setter());
      logging::event("physical", std::string("probe input=") + bool_text(request.set_probe_inputs().probe()) +
                                     " ets=" + bool_text(request.set_probe_inputs().tool_setter()));
      return ok(request.id());
    case Request::kGetProbeInputs: {
      const auto states = firmware_.probe_inputs();
      auto response = ok(request.id());
      auto* inputs = response.mutable_probe_inputs();
      inputs->set_probe(states.first);
      inputs->set_tool_setter(states.second);
      return response;
    }
    case Request::kSetCoverOpen:
      firmware_.set_cover_open(request.set_cover_open().open());
      logging::event("physical", std::string("cover ") + (request.set_cover_open().open() ? "open" : "closed"));
      return ok(request.id());
    case Request::kGetCoverOpen: {
      auto response = ok(request.id());
      response.mutable_cover_state()->set_open(firmware_.cover_open());
      return response;
    }
    case Request::kSetLimitSwitch: {
      const auto axis = api::axis_index(request.set_limit_switch().axis());
      const auto side = api::limit_switch_side(request.set_limit_switch().side());
      if (!axis.has_value() || !side.has_value()) {
        return error(request.id(), "invalid limit switch");
      }
      firmware_.set_limit_switch(*axis, *side, request.set_limit_switch().triggered());
      logging::event("physical", std::string("limit ") + api::axis_letter(request.set_limit_switch().axis()) +
                                     (*side == LimitSwitchSide::Min ? "-min " : "-max ") +
                                     bool_text(request.set_limit_switch().triggered()));
      return ok(request.id());
    }
    case Request::kGetLimitSwitch: {
      const auto axis = api::axis_index(request.get_limit_switch().axis());
      const auto side = api::limit_switch_side(request.get_limit_switch().side());
      if (!axis.has_value() || !side.has_value()) {
        return error(request.id(), "invalid limit switch");
      }
      auto response = ok(request.id());
      auto* state = response.mutable_limit_switch_state();
      state->set_axis(request.get_limit_switch().axis());
      state->set_side(request.get_limit_switch().side());
      state->set_triggered(firmware_.limit_switch(*axis, *side));
      return response;
    }
    case Request::kSetMotorAlarm: {
      const auto axis = api::axis_index(request.set_motor_alarm().axis());
      if (!axis.has_value()) {
        return error(request.id(), "invalid motor alarm axis");
      }
      firmware_.set_motor_alarm(*axis, request.set_motor_alarm().triggered());
      logging::event("fault", std::string("motor ") + api::axis_letter(request.set_motor_alarm().axis()) + " alarm " +
                                  bool_text(request.set_motor_alarm().triggered()));
      return ok(request.id());
    }
    case Request::kGetMotorAlarm: {
      const auto axis = api::axis_index(request.get_motor_alarm().axis());
      if (!axis.has_value()) {
        return error(request.id(), "invalid motor alarm axis");
      }
      auto response = ok(request.id());
      auto* state = response.mutable_motor_alarm_state();
      state->set_axis(request.get_motor_alarm().axis());
      state->set_triggered(firmware_.motor_alarm(*axis));
      return response;
    }
    case Request::kSetSpindleAlarm:
      if (!firmware_.set_spindle_alarm(request.set_spindle_alarm().triggered())) {
        return error(request.id(), "spindle alarm is not configured");
      }
      firmware_.run_main_loop(1);
      logging::event("fault", std::string("spindle alarm ") + bool_text(request.set_spindle_alarm().triggered()));
      return ok(request.id());
    case Request::kSetRotaryAccessoryInstalled:
      simulator_.set_rotary_accessory_installed(request.set_rotary_accessory_installed().installed());
      logging::event("physical", std::string("rotary accessory ") +
                                     bool_text(request.set_rotary_accessory_installed().installed()));
      return ok(request.id());
    case Request::kGetSpindleAlarm: {
      const auto alarm = firmware_.spindle_alarm();
      auto response = ok(request.id());
      auto* state = response.mutable_spindle_alarm_state();
      state->set_available(alarm.has_value());
      state->set_triggered(alarm.value_or(false));
      return response;
    }
    case Request::kSetMainButtonPressed:
      firmware_.set_main_button_pressed(request.set_main_button_pressed().pressed());
      firmware_.run_main_loop(1);
      logging::event("physical", std::string("main button ") +
                                     (request.set_main_button_pressed().pressed() ? "pressed" : "released"));
      return ok(request.id());
    case Request::kSetEStopPressed:
      firmware_.set_e_stop_pressed(request.set_e_stop_pressed().pressed());
      firmware_.run_main_loop(1);
      logging::event("physical",
                     std::string("e-stop ") + (request.set_e_stop_pressed().pressed() ? "pressed" : "released"));
      return ok(request.id());
    case Request::kGetFrontPanelState: {
      const auto panel = firmware_.front_panel_state();
      auto response = ok(request.id());
      fill_front_panel(*response.mutable_front_panel_state(), panel);
      return response;
    }
    case Request::kSetAtcPocketTools: {
      if (const auto message = apply_atc_pocket_tools(simulator_.context().physical_scene(), firmware_,
                                                      request.set_atc_pocket_tools())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    }
    case Request::kSetSpindleTool: {
      if (const auto message =
              apply_spindle_tool(simulator_.context().physical_scene(), request.set_spindle_tool())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    }
    case Request::kSetProbeToolInstalled:
      firmware_.set_probe_tool_installed(request.set_probe_tool_installed().installed());
      logging::event("physical",
                     std::string("probe tool ") + bool_text(request.set_probe_tool_installed().installed()));
      return ok(request.id());
    case Request::kSetToolSetterBox:
      apply_tool_setter_box(simulator_.context().physical_scene(), firmware_, request.set_tool_setter_box());
      return ok(request.id());
    case Request::kSetStockBox:
      apply_stock_box(simulator_.context().physical_scene(), firmware_, request.set_stock_box());
      return ok(request.id());
    case Request::kSetTemperature: {
      if (const auto message = apply_temperature(firmware_, request.set_temperature())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    }
    case Request::kSetSwitchState: {
      const auto name = api::machine_switch(request.set_switch_state().name());
      if (!name.has_value()) {
        return error(request.id(), "unsupported switch name");
      }
      const std::optional<float> value =
          request.set_switch_state().has_value()
              ? std::make_optional(static_cast<float>(request.set_switch_state().value()))
              : std::nullopt;
      if (!firmware_.set_switch_state(*name, request.set_switch_state().on(), value)) {
        return error(request.id(), "switch is not available");
      }
      std::ostringstream message;
      message << "switch " << switch_name(request.set_switch_state().name()) << ' '
              << bool_text(request.set_switch_state().on());
      if (value.has_value()) {
        message << " value=" << *value;
      }
      logging::event("physical", message.str());
      return ok(request.id());
    }
    case Request::kGetSwitchState: {
      const auto name = api::machine_switch(request.get_switch_state().name());
      if (!name.has_value()) {
        return error(request.id(), "unsupported switch name");
      }
      const auto state = firmware_.switch_state(*name);
      auto response = ok(request.id());
      auto* output = response.mutable_switch_state();
      output->set_name(request.get_switch_state().name());
      output->set_available(state.available);
      output->set_on(state.on);
      output->set_value(state.value);
      return response;
    }
    case Request::kGetLaserState: {
      const auto state = firmware_.laser_state();
      auto response = ok(request.id());
      auto* output = response.mutable_laser_state();
      output->set_available(state.available);
      output->set_mode(state.mode);
      output->set_firing(state.firing);
      output->set_testing(state.testing);
      output->set_power_percent(state.power_percent);
      output->set_scale_percent(state.scale_percent);
      return response;
    }
    default:
      return std::nullopt;
  }
}

void ApiService::fill_physical_io_snapshot(carvera::sim::v1::PhysicalIoSnapshot& snapshot) {
  snapshot.Clear();

  const auto probe = firmware_.probe_inputs();
  snapshot.mutable_probe_inputs()->set_probe(probe.first);
  snapshot.mutable_probe_inputs()->set_tool_setter(probe.second);
  snapshot.mutable_cover()->set_open(firmware_.cover_open());
  fill_front_panel(*snapshot.mutable_front_panel(), firmware_.front_panel_state());

  for (const auto axis : kSnapshotAxes) {
    const auto axis_index = api::axis_index(axis);
    if (!axis_index.has_value()) {
      continue;
    }
    auto* output = snapshot.add_motor_alarms();
    output->set_axis(axis);
    output->set_triggered(firmware_.motor_alarm(*axis_index));
  }

  const auto spindle_alarm = firmware_.spindle_alarm();
  snapshot.mutable_spindle_alarm()->set_available(spindle_alarm.has_value());
  snapshot.mutable_spindle_alarm()->set_triggered(spindle_alarm.value_or(false));

  for (const auto name : kSnapshotSwitches) {
    const auto switch_name = api::machine_switch(name);
    if (!switch_name.has_value()) {
      continue;
    }
    const auto state = firmware_.switch_state(*switch_name);
    auto* output = snapshot.add_switches();
    output->set_name(name);
    output->set_available(state.available);
    output->set_on(state.on);
    output->set_value(state.value);
  }

  const auto laser = firmware_.laser_state();
  snapshot.mutable_laser()->set_available(laser.available);
  snapshot.mutable_laser()->set_mode(laser.mode);
  snapshot.mutable_laser()->set_firing(laser.firing);
  snapshot.mutable_laser()->set_testing(laser.testing);
  snapshot.mutable_laser()->set_power_percent(laser.power_percent);
  snapshot.mutable_laser()->set_scale_percent(laser.scale_percent);

  for (const auto pin : kSnapshotPwmPins) {
    const auto state = simulator_.pwm_output(pin);
    auto* output = snapshot.add_pwm_outputs();
    fill_pin(*output->mutable_pin(), pin);
    output->set_configured(state.configured);
    output->set_duty(state.duty);
    output->set_period_us(state.period_us);
  }
}

std::optional<ApiService::Response> ApiService::handle_cooperative_physical_command(
    const carvera::sim::v1::Request& request) {
  using Request = carvera::sim::v1::Request;

  switch (request.command_case()) {
    case Request::kSetProbeInputs:
      firmware_.set_probe_inputs(request.set_probe_inputs().probe(), request.set_probe_inputs().tool_setter());
      logging::event("physical", std::string("probe input=") + bool_text(request.set_probe_inputs().probe()) +
                                     " ets=" + bool_text(request.set_probe_inputs().tool_setter()));
      return ok(request.id());
    case Request::kSetCoverOpen:
      firmware_.set_cover_open(request.set_cover_open().open());
      logging::event("physical", std::string("cover ") + (request.set_cover_open().open() ? "open" : "closed"));
      return ok(request.id());
    case Request::kSetLimitSwitch: {
      const auto axis = api::axis_index(request.set_limit_switch().axis());
      const auto side = api::limit_switch_side(request.set_limit_switch().side());
      if (!axis.has_value() || !side.has_value()) {
        return error(request.id(), "invalid limit switch");
      }
      firmware_.set_limit_switch(*axis, *side, request.set_limit_switch().triggered());
      logging::event("physical", std::string("limit ") + api::axis_letter(request.set_limit_switch().axis()) +
                                     (*side == LimitSwitchSide::Min ? "-min " : "-max ") +
                                     bool_text(request.set_limit_switch().triggered()));
      return ok(request.id());
    }
    case Request::kSetMotorAlarm: {
      const auto axis = api::axis_index(request.set_motor_alarm().axis());
      if (!axis.has_value()) {
        return error(request.id(), "invalid motor alarm axis");
      }
      firmware_.set_motor_alarm(*axis, request.set_motor_alarm().triggered());
      logging::event("fault", std::string("motor ") + api::axis_letter(request.set_motor_alarm().axis()) + " alarm " +
                                  bool_text(request.set_motor_alarm().triggered()));
      return ok(request.id());
    }
    case Request::kSetSpindleAlarm:
      if (!firmware_.set_spindle_alarm(request.set_spindle_alarm().triggered())) {
        return error(request.id(), "spindle alarm is not configured");
      }
      logging::event("fault", std::string("spindle alarm ") + bool_text(request.set_spindle_alarm().triggered()));
      return ok(request.id());
    case Request::kSetMainButtonPressed:
      firmware_.set_main_button_pressed(request.set_main_button_pressed().pressed());
      logging::event("physical", std::string("main button ") +
                                     (request.set_main_button_pressed().pressed() ? "pressed" : "released"));
      return ok(request.id());
    case Request::kSetEStopPressed:
      firmware_.set_e_stop_pressed(request.set_e_stop_pressed().pressed());
      logging::event("physical",
                     std::string("e-stop ") + (request.set_e_stop_pressed().pressed() ? "pressed" : "released"));
      return ok(request.id());
    case Request::kSetRotaryAccessoryInstalled:
      simulator_.set_rotary_accessory_installed(request.set_rotary_accessory_installed().installed());
      logging::event("physical", std::string("rotary accessory ") +
                                     bool_text(request.set_rotary_accessory_installed().installed()));
      return ok(request.id());
    case Request::kSetAtcPocketTools:
      if (const auto message = apply_atc_pocket_tools(simulator_.context().physical_scene(), firmware_,
                                                      request.set_atc_pocket_tools())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    case Request::kSetSpindleTool:
      if (const auto message =
              apply_spindle_tool(simulator_.context().physical_scene(), request.set_spindle_tool())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    case Request::kSetToolSetterBox:
      apply_tool_setter_box(simulator_.context().physical_scene(), firmware_, request.set_tool_setter_box());
      return ok(request.id());
    case Request::kSetStockBox:
      apply_stock_box(simulator_.context().physical_scene(), firmware_, request.set_stock_box());
      return ok(request.id());
    case Request::kSetTemperature:
      if (const auto message = apply_temperature(firmware_, request.set_temperature())) {
        return error(request.id(), *message);
      }
      return ok(request.id());
    default:
      return std::nullopt;
  }
}

}  // namespace sim
