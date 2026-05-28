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
    status->active_tool = 1;
    status->target_tool = 1;
    status->tool_offset = 0.0F;
    request->set_taken();
  }
};

}  // namespace

int main() {
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

  return 0;
}
