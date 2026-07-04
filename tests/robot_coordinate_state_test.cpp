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

#include <array>
#include <string>

#include "Gcode.h"
#include "Robot.h"
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

void require_position(const Robot::wcs_t& position, const std::array<float, 5>& expected, std::string_view message) {
  require_near(std::get<0>(position), expected[0], 1.0e-5, message);
  require_near(std::get<1>(position), expected[1], 1.0e-5, message);
  require_near(std::get<2>(position), expected[2], 1.0e-5, message);
  require_near(std::get<3>(position), expected[3], 1.0e-5, message);
  require_near(std::get<4>(position), expected[4], 1.0e-5, message);
}

void send(Robot& robot, const char* command) {
  Gcode gcode(command, &StreamOutput::NullStream);
  robot.on_gcode_received(&gcode);
}

}  // namespace

int main() {
  sim::test::DirectRobotHarness harness({
      "load_last_wcs true\n",
      "save_g54 true\n",
      "save_g92 true\n",
  });
  auto& kernel = harness.kernel;
  auto& robot = *kernel.robot;

  kernel.eeprom_data->current_wcs = 1;
  kernel.eeprom_data->WCScoord[1][0] = 10.0F;
  kernel.eeprom_data->WCScoord[1][1] = 20.0F;
  kernel.eeprom_data->WCScoord[1][2] = 30.0F;
  kernel.eeprom_data->WCScoord[1][3] = 40.0F;
  kernel.eeprom_data->WCSrotation[1] = 0.0F;
  kernel.eeprom_data->tool_not_calibrated = false;
  harness.load_robot();

  require(robot.get_current_wcs() == 1, "common Robot startup should restore the persisted G55 selection when enabled");
  auto state = robot.get_wcs_state();
  require(state.size() == MAX_WCS + 3,
          "Robot WCS state should contain metadata, all workspaces, G92, and tool offsets");
  require(std::get<0>(state.front()) == 1.0F && std::get<1>(state.front()) == static_cast<float>(MAX_WCS),
          "Robot WCS metadata should publish the selected workspace and supported count");
  require_position(state[2], {10.0F, 20.0F, 30.0F, 40.0F, 0.0F},
                   "persisted G55 offsets should load from machine EEPROM");

  const Robot::wcs_t machine_position{11.0F, 22.0F, 33.0F, 44.0F, 5.0F};
  const auto workspace_position = robot.mcs2wcs(machine_position);
  require_position(workspace_position, {1.0F, 2.0F, 3.0F, 4.0F, 5.0F},
                   "machine-to-workspace conversion should subtract the selected offsets");
  require_position(robot.wcs2mcs(workspace_position), {11.0F, 22.0F, 33.0F, 44.0F, 5.0F},
                   "workspace conversion should round-trip through machine coordinates");

  const std::array<std::pair<const char*, const char*>, 6> position_reports = {{
      {"M114", "C: X:-10.0000"},
      {"M114.1", "WCS: X:-10.0000"},
      {"M114.2", "MCS: X:0.0000"},
      {"M114.3", "APOS: X:0.0000"},
      {"M114.4", "MP: X:0.0000"},
      {"M114.5", "CMP: X:0.0000"},
  }};
  for (const auto& [command, expected] : position_reports) {
    Gcode report(command, &StreamOutput::NullStream);
    robot.on_gcode_received(&report);
    require_contains(report.txt_after_ok, expected,
                     "M114 variants should expose their documented Robot coordinate view");
  }
  Gcode actuator_report("M114.2", &StreamOutput::NullStream);
  robot.on_gcode_received(&actuator_report);
  require_contains(actuator_report.txt_after_ok, " A:0.0000",
                   "machine-coordinate reporting should include the configured C1 rotary axes");

  send(robot, "G10 L2 P2 R90 X10 Y20 Z30 A40 B50");
  const Robot::wcs_t rotated_machine{8.0F, 21.0F, 33.0F, 44.0F, 55.0F};
  const auto rotated_workspace = robot.mcs2wcs(rotated_machine);
  require_position(rotated_workspace, {1.0F, 2.0F, 3.0F, 4.0F, 5.0F},
                   "G10 L2 rotation should participate in inverse workspace conversion");
  require_position(robot.wcs2mcs(rotated_workspace), {8.0F, 21.0F, 33.0F, 44.0F, 55.0F},
                   "rotated G55 coordinates should round-trip without drift");

  send(robot, "G10 L20 P3 X5 Y6 Z7 A8 B9");
  require_position(robot.mcs2selected_wcs(Robot::wcs_t{0, 0, 0, 0, 0}, 2), {5, 6, 7, 8, 9},
                   "G10 L20 should make the current machine position equal the requested workspace position");

  send(robot, "G59.3");
  require(robot.get_current_wcs() == 8, "G59.3 should select the ninth supported workspace");
  require(kernel.eeprom_data->current_wcs == 0,
          "non-persisted extended workspaces should leave the EEPROM selection at G54");
  send(robot, "G55");
  require(robot.get_current_wcs() == 1 && kernel.eeprom_data->current_wcs == 1,
          "G55 selection should update both live Robot state and persisted selection");

  robot.set_current_wcs_by_mpos(2.0F, NAN, 4.0F, 5.0F, NAN, 30.0F);
  state = robot.get_wcs_state();
  require_position(state[2], {2.0F, 20.0F, 4.0F, 5.0F, 50.0F},
                   "probe-style partial WCS updates should preserve unspecified axes");
  require_near(robot.r[1], 30.0F, 1.0e-5, "probe-style WCS updates should persist the measured rotation");

  send(robot, "G92 X7 Y8 Z9 A10 B11");
  require_position(robot.mcs2wcs(robot.get_axis_position()), {7, 8, 9, 10, 11},
                   "G92 should assign the requested workspace coordinates to the current machine position");

  CapturingStream settings_stream;
  Gcode settings("M503", &settings_stream);
  robot.on_gcode_received(&settings);
  require_contains(settings_stream.output, ";WCS settings", "M503 should emit persistent Robot workspace settings");
  require_contains(settings_stream.output, "G10 L2 P2", "M503 should serialize the configured G55 offsets");
  require_contains(settings_stream.output, "G92.3", "M503 should serialize a nonzero persistent G92 offset");

  send(robot, "G92.1");
  state = robot.get_wcs_state();
  require_position(state[MAX_WCS + 1], {0, 0, 0, 0, 0}, "G92.1 should clear every temporary coordinate offset");

  send(robot, "M120");
  send(robot, "G91");
  send(robot, "G93");
  send(robot, "G54");
  require(!robot.absolute_mode && robot.inverse_time_mode && robot.get_current_wcs() == 0,
          "test should alter modal Robot state before restoring it");
  send(robot, "M121");
  require(robot.absolute_mode && !robot.inverse_time_mode && robot.get_current_wcs() == 1,
          "M121 should restore positioning, feed, and workspace modes saved by M120");

  send(robot, "G91");
  send(robot, "G93");
  send(robot, "M2");
  require(robot.absolute_mode && !robot.inverse_time_mode,
          "end-of-program handling should restore absolute positioning and normal feed mode");

  send(robot, "M83");
  require(!robot.e_absolute_mode, "M83 should select relative auxiliary-axis positioning");
  send(robot, "M82");
  require(robot.e_absolute_mode, "M82 should restore absolute auxiliary-axis positioning");

  return 0;
}
