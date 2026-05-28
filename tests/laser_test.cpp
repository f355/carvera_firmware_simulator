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

#include <cmath>

#include "test_support.hpp"

#include "Config.h"
#include "Gcode.h"
#include "LaserPublicAccess.h"
#include "PublicData.h"
#include "PwmOut.h"
#include "Robot.h"
#include "SerialMessage.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "lpc1768_sim.h"
#include "modules/tools/laser/Laser.h"

extern "C" void TIMER2_IRQHandler(void);

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

bool pin_is_high(std::uint8_t port, std::uint8_t pin) {
  return (sim::lpc1768::gpio_port(port).FIOPIN & (1u << pin)) != 0;
}

void run_slow_ticker_hook() {
  TIMER2_IRQHandler();
  TIMER2_IRQHandler();
}

}  // namespace

int main() {
  sim::lpc1768::reset();
  mbed::PwmOut::reset_states();
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "laser_module_enable true\n",
      "laser_module_pin 2.12\n",
      "laser_module_pwm_pin 2.4\n",
      "laser_module_ttl_pin nc\n",
      "laser_module_pwm_period 1000\n",
      "laser_module_test_power 0.01\n",
      "laser_module_maximum_power 1.0\n",
      "laser_module_minimum_power 0.0\n",
  }));
  kernel.config->config_cache_load();

  auto* laser = new Laser();
  kernel.add_module(laser);

  auto pwm = mbed::PwmOut::state(P2_4);
  require(pwm.configured, "Laser should configure the PWM pin from firmware config");
  require(pwm.period_us == 1000.0F, "Laser should propagate the configured PWM period");
  require(pwm.duty == 0.0F, "Laser PWM should start off");
  require(!pin_is_high(2, 12), "Laser enable pin should start low");

  SerialMessage on_message{&StreamOutput::NullStream, "laser on", 1};
  kernel.call_event(ON_CONSOLE_LINE_RECEIVED, &on_message);
  require(kernel.get_laser_mode(), "laser on should enter laser mode");
  require(pin_is_high(2, 12), "laser on should raise the laser enable GPIO");

  laser_status status{};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &status),
          "Laser should publish status through PublicData");
  require(status.mode, "published laser status should report laser mode");
  require(!status.state, "console laser on should not report active M3 laser firing");
  require(!status.testing, "laser test mode should start disabled");
  require(std::fabs(status.scale - 100.0F) < 0.001F, "laser scale should default to 100 percent");

  Gcode fire("M3 S500", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &fire);
  status = {};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &status),
          "Laser should keep publishing status while firing");
  require(status.state, "M3 in laser mode should report laser firing");
  require(!status.testing, "M3 in laser mode should leave test mode disabled");
  require(pin_is_high(2, 12), "M3 in laser mode should raise the laser enable GPIO");

  Gcode stop("M5", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &stop);
  status = {};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &status),
          "Laser should keep publishing status after M5");
  require(!status.state, "M5 should report laser firing off");
  require(!pin_is_high(2, 12), "M5 should lower the laser enable GPIO");

  Gcode test_on("M323", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &test_on);
  run_slow_ticker_hook();

  status = {};
  require(PublicData::get_value(laser_checksum, get_laser_status_checksum, &status),
          "Laser should keep publishing status while testing");
  require(status.testing, "M323 should enter laser test mode");
  pwm = mbed::PwmOut::state(P2_4);
  require(std::fabs(pwm.duty - 0.01F) < 0.0001F, "laser test mode should drive configured test PWM power");

  Gcode scale("M325 S50", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &scale);
  run_slow_ticker_hook();
  pwm = mbed::PwmOut::state(P2_4);
  require(std::fabs(pwm.duty - 0.005F) < 0.0001F, "M325 should scale laser test PWM power");

  Gcode test_off("M324", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &test_off);
  run_slow_ticker_hook();
  require(!pin_is_high(2, 12), "M324 should lower the laser enable GPIO");
  pwm = mbed::PwmOut::state(P2_4);
  require(pwm.duty == 0.0F, "M324 should turn laser PWM off on the next slow tick");

  SerialMessage off_message{&StreamOutput::NullStream, "laser off", 2};
  kernel.call_event(ON_CONSOLE_LINE_RECEIVED, &off_message);
  require(!kernel.get_laser_mode(), "laser off should return to CNC mode");

  Gcode full_laser_mode("M321", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &full_laser_mode);
  require(kernel.get_laser_mode(), "full M321 should enter laser mode");
  auto zero_wcs = kernel.robot->mcs2wcs(Robot::wcs_t(0, 0, 0, 0, 0));
  require(std::fabs(std::get<X_AXIS>(zero_wcs)) > 0.001F, "full M321 should apply Robot's laser X offset");
  require(std::fabs(std::get<Y_AXIS>(zero_wcs)) > 0.001F, "full M321 should apply Robot's laser Y offset");
  require(std::fabs(std::get<Z_AXIS>(zero_wcs)) > 0.001F, "full M321 should apply Robot's laser Z offset");

  Gcode full_cnc_mode("M322", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &full_cnc_mode);
  require(!kernel.get_laser_mode(), "full M322 should return to CNC mode");
  zero_wcs = kernel.robot->mcs2wcs(Robot::wcs_t(0, 0, 0, 0, 0));
  require(std::fabs(std::get<X_AXIS>(zero_wcs)) < 0.001F, "full M322 should clear Robot's laser X offset");
  require(std::fabs(std::get<Y_AXIS>(zero_wcs)) < 0.001F, "full M322 should clear Robot's laser Y offset");
  require(std::fabs(std::get<Z_AXIS>(zero_wcs)) < 0.001F, "full M322 should clear Robot's laser Z offset");

  return 0;
}
