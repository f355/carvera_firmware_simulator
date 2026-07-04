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

#include "test_support.hpp"

#include "ATCHandlerPublicAccess.h"
#include "Config.h"
#include "Gcode.h"
#include "PublicData.h"
#include "PublicDataRequest.h"
#include "PwmOut.h"
#include "SpindlePublicAccess.h"
#include "libs/Kernel.h"
#include "lpc1768_sim.h"
#include "modules/tools/spindle/SpindleMaker.h"

extern "C" void TIMER2_IRQHandler(void);

#include "sim/machine_simulator.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

class ToolStatusModule : public Module {
 public:
  void on_get_public_data(void* argument) override {
    auto* request = static_cast<PublicDataRequest*>(argument);
    if (!request->starts_with(atc_handler_checksum) || !request->second_element_is(get_tool_status_checksum)) {
      return;
    }

    auto* status = static_cast<tool_status*>(request->get_data_ptr());
    status->active_tool = active_tool;
    status->target_tool = active_tool;
    status->tool_offset = 0.0F;
    request->set_taken();
  }

  int active_tool{1};
};

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::lpc1768::reset();
  mbed::PwmOut::reset_states();
  Kernel kernel;
  ToolStatusModule tool_status;

  kernel.config = new Config(new MemoryConfigSource({
      "spindle.enable true\n",
      "spindle.type pwm\n",
      "spindle.pwm_pin 2.0\n",
      "spindle.feedback_pin 2.6\n",
      "spindle.alarm_pin nc\n",
      "spindle.pwm_period 250\n",
      "spindle.default_rpm 8000\n",
      "spindle.max_rpm 12000\n",
      "spindle.delay_s 0\n",
      "spindle.control_P 0.0002\n",
  }));
  kernel.config->config_cache_load();
  kernel.register_for_event(ON_GET_PUBLIC_DATA, &tool_status);

  SpindleMaker maker;
  maker.load_spindle();

  auto pwm = mbed::PwmOut::state(P2_0);
  require(pwm.configured, "spindle should configure the PWM pin from firmware config");
  require(pwm.period_us == 250.0F, "spindle should propagate configured PWM period");
  require(pwm.duty == 0.0F, "spindle should start with zero duty");

  Gcode start("M3 S6000", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &start);
  require(kernel.spindleon, "M3 should turn on the real firmware spindle module");

  spindle_status status{};
  require(PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status),
          "PWM spindle should publish status through PublicData");
  require(status.state, "published spindle state should be on after M3");
  require(status.target_rpm == 6000.0F, "published spindle target should reflect M3 S value");

  Gcode capped_speed("M3 S50000", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &capped_speed);
  status = {};
  require(PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status),
          "PWM spindle status should remain available after a speed change");
  require(status.target_rpm == 12000.0F, "spindle target should clamp to its configured maximum RPM");

  Gcode minimum_override("M223 S5", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &minimum_override);
  status = {};
  PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status);
  require(status.factor == 10.0F, "spindle override should clamp to its ten-percent minimum");

  Gcode maximum_override("M223 S500", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &maximum_override);
  status = {};
  PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status);
  require(status.factor == 300.0F, "spindle override should clamp to its three-hundred-percent maximum");

  spindle_status requested_status{};
  requested_status.factor = 125.0F;
  require(PublicData::set_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &requested_status),
          "other firmware modules should be able to set spindle override through PublicData");
  status = {};
  PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status);
  require(status.factor == 125.0F, "PublicData spindle override should update the published factor");

  for (int i = 0; i < 25; ++i) {
    TIMER2_IRQHandler();
  }
  pwm = mbed::PwmOut::state(P2_0);
  require(pwm.duty > 0.0F, "spindle idle update should drive non-zero PWM duty while on");

  Gcode stop("M5", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &stop);
  require(!kernel.spindleon, "M5 should turn off the real firmware spindle module");

  status = {};
  require(PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status),
          "PWM spindle should continue publishing status after M5");
  require(!status.state, "published spindle state should be off after M5");

  kernel.call_event(ON_GCODE_RECEIVED, &start);
  require(kernel.spindleon, "spindle should restart after a normal stop");
  require(PublicData::set_value(pwm_spindle_control_checksum, turn_off_spindle_checksum, nullptr),
          "ATC-style PublicData request should turn the spindle off");
  require(!kernel.spindleon, "PublicData spindle shutdown should update the kernel spindle state");

  kernel.call_event(ON_GCODE_RECEIVED, &start);
  kernel.call_event(ON_HALT, nullptr);
  require(!kernel.spindleon, "entering a firmware halt should stop a running spindle");

  kernel.set_halted(false);
  tool_status.active_tool = 0;
  kernel.call_event(ON_GCODE_RECEIVED, &start);
  require(kernel.is_halted(), "firmware should halt rather than start the spindle without a valid tool");
  require(kernel.get_halt_reason() == MANUAL, "invalid-tool spindle start should report the manual halt reason");

  status = {};
  PublicData::get_value(pwm_spindle_control_checksum, get_spindle_status_checksum, &status);
  require(!status.state, "invalid-tool protection should leave the spindle stopped");

  return 0;
}
