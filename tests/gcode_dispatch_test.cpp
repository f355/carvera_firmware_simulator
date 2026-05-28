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

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "Conveyor.h"
#include "GcodeDispatch.h"
#include "Robot.h"
#include "SerialMessage.h"
#include "StepTicker.h"
#include "StreamOutput.h"
#include "libs/Kernel.h"
#include "sim/host_filesystem.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/motion_runner.hpp"
#include "support/direct_robot_config.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

class CapturingStream : public StreamOutput {
 public:
  int puts(const char* buf, int size = 0) override {
    output.append(buf, size == 0 ? std::char_traits<char>::length(buf) : static_cast<size_t>(size));
    return size;
  }

  std::string output;
};

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "carvera_sim_gcode_dispatch_test";
  std::filesystem::remove_all(root);
  sim::test::write_direct_robot_config(root);
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);
  sim::i2c_eeprom::reset();
  sim::i2c_eeprom::configure_factory_settings({sim::MachineModel::CarveraC1, 0x04});

  sim::MachineSimulator simulator;
  Kernel kernel;

  const auto axis = simulator.add_step_dir_axis({1, 18}, {1, 20});
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();

  CapturingStream stream;
  SerialMessage relative_mode{&stream, "G91", 1};
  kernel.gcode_dispatch->on_console_line_received(&relative_mode);

  SerialMessage jog{&stream, "G0 X5 F1500", 2};
  kernel.gcode_dispatch->on_console_line_received(&jog);

  sim::MotionRunner runner(kernel);
  require(runner.run_until_idle(50'000), "simulator should execute dispatched G-code motion to idle");
  require(simulator.axis_position_steps(axis) == 50,
          "physical axis position should reflect dispatched G-code step/dir pulses");
  require(kernel.robot->get_axis_position(0) == 5.0F, "Robot should update position from dispatched G-code jog");
  require(stream.output.find("ok") != std::string::npos, "GcodeDispatch should acknowledge accepted commands");

  stream.output.clear();
  SerialMessage firmware_version{&stream, "M115", 3};
  kernel.gcode_dispatch->on_console_line_received(&firmware_version);
  require(stream.output.find("FIRMWARE_NAME:Smoothieware") != std::string::npos,
          "real GcodeDispatch should answer M115 firmware version queries");

  stream.output.clear();
  SerialMessage combined_line{&stream, "G91 G0 X2 F1500", 4};
  kernel.gcode_dispatch->on_console_line_received(&combined_line);
  require(runner.run_until_idle(50'000), "simulator should execute combined-line G-code motion to idle");
  require(simulator.axis_position_steps(axis) == 70,
          "real GcodeDispatch should split a modal setup plus move on one line");

  stream.output.clear();
  SerialMessage unsupported_units{&stream, "G20", 5};
  kernel.gcode_dispatch->on_console_line_received(&unsupported_units);
  require(kernel.is_halted(), "unsupported units should put the firmware into halt/alarm");

  SerialMessage recover{&stream, "M999", 6};
  kernel.gcode_dispatch->on_console_line_received(&recover);
  require(!kernel.is_halted(), "real GcodeDispatch should unlock halt with M999");

  SerialMessage post_recovery_move{&stream, "G0 X1 F1500", 7};
  kernel.gcode_dispatch->on_console_line_received(&post_recovery_move);
  require(runner.run_until_idle(50'000), "simulator should execute motion after M999 recovery");
  require(simulator.axis_position_steps(axis) == 80, "M999 recovery should allow subsequent G-code motion");

  std::filesystem::remove_all(root);
  return 0;
}
