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
#include <cstdint>
#include <string>
#include <vector>

#define private public
#include "Planner.h"
#undef private

#include "Gcode.h"
#include "EndstopsPublicAccess.h"
#include "PublicDataRequest.h"
#include "Robot.h"
#include "StepperMotor.h"
#include "StreamOutput.h"
#include "support/assertions.hpp"
#include "support/direct_robot_harness.hpp"

namespace {

using sim::test::require;
using sim::test::require_contains;
using sim::test::require_near;

class CapturingStream : public StreamOutput {
 public:
  int puts(const char* buffer, int size = 0) override {
    output.append(buffer, size == 0 ? std::char_traits<char>::length(buffer) : static_cast<std::size_t>(size));
    return size;
  }

  std::string output;
};

class EnableCapture : public Module {
 public:
  void on_enable(void* argument) override { masks.push_back(reinterpret_cast<std::uintptr_t>(argument)); }

  std::vector<std::uintptr_t> masks;
};

class HomingStatusModule : public Module {
 public:
  void on_get_public_data(void* argument) override {
    auto* request = static_cast<PublicDataRequest*>(argument);
    if (!request->starts_with(endstops_checksum)) {
      return;
    }
    if (request->second_element_is(get_homing_status_checksum)) {
      *static_cast<bool*>(request->get_data_ptr()) = false;
      request->set_taken();
    } else if (request->second_element_is(get_homed_status_checksum)) {
      auto* homed = static_cast<bool*>(request->get_data_ptr());
      homed[0] = homed[1] = homed[2] = true;
      request->set_taken();
    }
  }
};

void send(Robot& robot, const char* command) {
  Gcode gcode(command, &StreamOutput::NullStream);
  robot.on_gcode_received(&gcode);
}

void test_runtime_settings() {
  sim::test::DirectRobotHarness harness;
  auto& kernel = harness.kernel;
  auto& robot = *kernel.robot;
  harness.load_robot();

  Gcode steps("M92 X20 Y30 Z40 A50 B60", &StreamOutput::NullStream);
  robot.on_gcode_received(&steps);
  const std::array<float, 5> expected_steps = {20, 30, 40, 50, 60};
  for (std::size_t axis = 0; axis < expected_steps.size(); ++axis) {
    require_near(robot.actuators[axis]->get_steps_per_mm(), expected_steps[axis], 1.0e-6,
                 "M92 should update configured C1 actuator calibration");
  }

  send(robot, "M203 X12 Y13 Z14 S15");
  require_near(robot.get_z_maxfeedrate(), 14.0, 1.0e-6, "M203 should update the Cartesian Z feed-rate limit");

  send(robot, "M203.1 X20 Y21 Z22 A23 B24");
  const std::array<float, 5> expected_rates = {20, 21, 22, 23, 24};
  for (std::size_t axis = 0; axis < expected_rates.size(); ++axis) {
    require_near(robot.actuators[axis]->get_max_rate(), expected_rates[axis], 1.0e-6,
                 "M203.1 should update per-actuator maximum rates");
  }

  send(robot, "M204 S0 X0 Y200 A300");
  require_near(robot.get_default_acceleration(), 1.0, 1.0e-6, "M204 should enforce the minimum default acceleration");
  require(std::isnan(robot.actuators[0]->get_acceleration()),
          "nonpositive per-axis acceleration should select the default acceleration");
  require_near(robot.actuators[1]->get_acceleration(), 200.0, 1.0e-6,
               "M204 should update a Cartesian actuator acceleration");
  require_near(robot.actuators[3]->get_acceleration(), 300.0, 1.0e-6,
               "M204 should update the C1 rotary actuator acceleration");

  send(robot, "M205 X-1 Z-1 S-2");
  require_near(kernel.planner->junction_deviation, 0.0, 1.0e-6,
               "M205 should clamp negative junction deviation to zero");
  require(std::isnan(kernel.planner->z_junction_deviation),
          "M205 Z-1 should disable the dedicated Z junction deviation");
  require_near(kernel.planner->minimum_planner_speed, 0.0, 1.0e-6,
               "M205 should clamp negative minimum planner speed to zero");

  send(robot, "M220 S5");
  require_near(robot.get_seconds_per_minute(), 600.0, 1.0e-6, "M220 should clamp motion override to ten percent");
  send(robot, "M220 S2000");
  require_near(robot.get_seconds_per_minute(), 6.0, 1.0e-6,
               "M220 should clamp motion override to one thousand percent");
  send(robot, "M220 S250");
  require_near(robot.get_seconds_per_minute(), 24.0, 1.0e-6, "M220 should retain an in-range motion override");

  send(robot, "M211 S1");
  require(robot.is_soft_endstop_enabled(), "M211 S1 should enable Robot soft limits at runtime");
  send(robot, "M211 S0");
  require(!robot.is_soft_endstop_enabled(), "M211 S0 should support the CA1 configuration's disabled soft-limit state");

  EnableCapture enables;
  kernel.register_for_event(ON_ENABLE, &enables);
  send(robot, "M17");
  send(robot, "M18 X0 A0");
  send(robot, "M84");
  require(enables.masks.size() == 3, "Robot motor enable commands should emit one enable event each");
  require(enables.masks[0] == 1, "M17 should request all motor enables");
  require(enables.masks[1] == ((0x02U << 0U) | (0x02U << 3U)),
          "M18 should encode the requested C1 X and A motors in its disable mask");
  require(enables.masks[2] == 0, "M84 should request that every motor be disabled");

  CapturingStream report_stream;
  Gcode rate_report("M203.1", &report_stream);
  robot.on_gcode_received(&rate_report);
  require_contains(report_stream.output, " A: 23", "M203.1 reporting should include the C1 rotary actuator");

  report_stream.output.clear();
  Gcode override_report("M220", &report_stream);
  robot.on_gcode_received(&override_report);
  require_contains(report_stream.output, "250.00 %", "M220 without S should report the active motion override");
}

void test_motion_and_limits() {
  sim::test::DirectRobotHarness harness({
      "soft_endstop.enable true\n",
      "soft_endstop.halt true\n",
      "soft_endstop.x_min -10\n",
      "soft_endstop.y_min -10\n",
      "soft_endstop.z_min -10\n",
      "mm_per_arc_segment 1.0\n",
  });
  auto& kernel = harness.kernel;
  auto& robot = *kernel.robot;
  harness.load_robot();
  HomingStatusModule homing_status;
  kernel.register_for_event(ON_GET_PUBLIC_DATA, &homing_status);

  send(robot, "G91");
  Gcode allowed("G1 X-6 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&allowed);
  require(harness.run_until_idle().status == sim::EventRunStatus::ConditionReached,
          "C1 should execute a move that remains inside its enabled soft limit");
  require_near(robot.get_axis_position(X_AXIS), -6.0, 1.0e-5,
               "accepted C1 motion should update the Robot machine position");

  Gcode rejected("G1 X-4.1 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&rejected);
  require(kernel.is_halted(), "C1 should halt when commanded beyond its enabled X soft limit");
  require(kernel.get_halt_reason() == SOFT_LIMIT, "C1 soft-limit violation should publish the firmware fault reason");
  require_near(robot.get_axis_position(X_AXIS), -6.0, 1.0e-5,
               "a rejected soft-limit move should not advance the Robot position");
  require(harness.simulator.axis_position_steps(harness.axis_ids[X_AXIS]) == -60,
          "a rejected soft-limit move should not generate physical steps");

  kernel.set_halted(false);
  send(robot, "M211 S0");
  Gcode ca1_style_move("G1 X-4.1 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&ca1_style_move);
  require(harness.run_until_idle().status == sim::EventRunStatus::ConditionReached,
          "the CA1-style disabled soft-limit state should allow the same move");
  require_near(robot.get_axis_position(X_AXIS), -10.1, 1.0e-5,
               "motion with soft limits disabled should reach its requested endpoint");

  Gcode zero_feed("G1 X1 F0", &StreamOutput::NullStream);
  robot.on_gcode_received(&zero_feed);
  require(zero_feed.is_error, "Robot should reject a zero feed rate before queuing motion");
  require_contains(zero_feed.txt_after_ok, "Undefined feed rate",
                   "zero-feed rejection should explain the invalid command");

  Gcode negative_feed("G1 X1 F-1", &StreamOutput::NullStream);
  robot.on_gcode_received(&negative_feed);
  require(negative_feed.is_error, "Robot should reject a negative feed rate before queuing motion");
  require_contains(negative_feed.txt_after_ok, "feed rate < 0",
                   "negative-feed rejection should explain the invalid command");

  send(robot, "G93");
  Gcode missing_inverse_feed("G1 X1", &StreamOutput::NullStream);
  robot.on_gcode_received(&missing_inverse_feed);
  require(kernel.is_halted(), "G93 motion without an explicit F value should halt before planning");
  require(kernel.get_halt_reason() == MANUAL,
          "missing inverse-time feed should use the firmware's manual-input halt reason");
  require_near(robot.get_axis_position(X_AXIS), -10.1, 1.0e-5,
               "rejected inverse-time motion should leave the Robot position unchanged");
  kernel.set_halted(false);
  send(robot, "G94");

  Gcode full_circle("G2 I1 J0 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&full_circle);
  require(!full_circle.is_error, "Robot should accept a full-circle XY arc with an unchanged endpoint");
  require(harness.run_until_idle(500'000).status == sim::EventRunStatus::ConditionReached,
          "Robot should execute a segmented full-circle arc to idle");
  require_near(robot.get_axis_position(X_AXIS), -10.1, 1.0e-4,
               "full-circle arc should return to its starting X coordinate");
  require_near(robot.get_axis_position(Y_AXIS), 0.0, 1.0e-4,
               "full-circle arc should return to its starting Y coordinate");

  send(robot, "G18");
  Gcode xz_arc("G2 X2 Z0 I1 K0 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&xz_arc);
  require(!xz_arc.is_error, "Robot should accept a C1/CA1 XZ-plane arc");
  require(harness.run_until_idle(500'000).status == sim::EventRunStatus::ConditionReached,
          "Robot should execute an XZ-plane arc to idle");
  require_near(robot.get_axis_position(X_AXIS), -8.1, 1.0e-4, "XZ-plane arc should reach its relative X endpoint");

  send(robot, "G19");
  Gcode yz_arc("G3 Y2 Z0 J1 K0 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&yz_arc);
  require(!yz_arc.is_error, "Robot should accept a C1/CA1 YZ-plane arc");
  require(harness.run_until_idle(500'000).status == sim::EventRunStatus::ConditionReached,
          "Robot should execute a YZ-plane arc to idle");
  require_near(robot.get_axis_position(Y_AXIS), 2.0, 1.0e-4, "YZ-plane arc should reach its relative Y endpoint");

  send(robot, "G17");
  send(robot, "G90");
  send(robot, "G10 L2 P1 X10 Y10 Z0");
  send(robot, "G54");
  Gcode workspace_origin("G0 X0 Y0 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&workspace_origin);
  require(harness.run_until_idle(500'000).status == sim::EventRunStatus::ConditionReached,
          "ordinary absolute motion should apply the active G54 offset");
  require_near(robot.get_axis_position(X_AXIS), 10.0, 1.0e-4,
               "G54 workspace origin should map to its configured machine X offset");
  require_near(robot.get_axis_position(Y_AXIS), 10.0, 1.0e-4,
               "G54 workspace origin should map to its configured machine Y offset");

  robot.next_command_is_MCS = true;
  Gcode machine_origin("G0 X0 Y0 F600", &StreamOutput::NullStream);
  robot.on_gcode_received(&machine_origin);
  require(harness.run_until_idle(500'000).status == sim::EventRunStatus::ConditionReached,
          "G53-style machine-coordinate motion should execute to idle");
  require(!robot.next_command_is_MCS, "Robot should consume the G53 machine-coordinate flag on the following move");
  require_near(robot.get_axis_position(X_AXIS), 0.0, 1.0e-4, "G53 should bypass G54 and target machine X directly");
  require_near(robot.get_axis_position(Y_AXIS), 0.0, 1.0e-4, "G53 should bypass G54 and target machine Y directly");
}

}  // namespace

int main() {
  test_runtime_settings();
  test_motion_and_limits();
  return 0;
}
