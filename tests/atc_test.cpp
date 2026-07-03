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
#include <fstream>
#include <iostream>
#include <string>

#include "ATCHandlerPublicAccess.h"
#include "Conveyor.h"
#include "Gcode.h"
#include "Module.h"
#include "PublicData.h"
#include "PublicDataRequest.h"
#include "Robot.h"
#include "SlowTicker.h"
#include "StepTicker.h"
#include "StreamOutputPool.h"
#include "libs/Kernel.h"
#include "modules/tools/atc/ATCHandler.h"
#include "sim/machine_simulator.hpp"
#include "sim/motion_pump.hpp"
#include "sim/persistent_machine_state.hpp"
#include "sim/robot_axis_binding.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void write_atc_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "arm_solution cartesian\n"
         << "alpha_step_pin 1.18\n"
         << "alpha_dir_pin 1.20\n"
         << "alpha_en_pin nc\n"
         << "alpha_steps_per_mm 10\n"
         << "alpha_max_rate 6000\n"
         << "alpha_acceleration 1000\n"
         << "beta_step_pin 1.19\n"
         << "beta_dir_pin 1.21\n"
         << "beta_en_pin nc\n"
         << "beta_steps_per_mm 10\n"
         << "beta_max_rate 6000\n"
         << "beta_acceleration 1000\n"
         << "gamma_step_pin 1.22\n"
         << "gamma_dir_pin 1.23\n"
         << "gamma_en_pin nc\n"
         << "gamma_steps_per_mm 10\n"
         << "gamma_max_rate 6000\n"
         << "gamma_acceleration 1000\n"
         << "delta_step_pin 1.24\n"
         << "delta_dir_pin 1.25\n"
         << "delta_en_pin nc\n"
         << "delta_steps_per_mm 10\n"
         << "delta_max_rate 6000\n"
         << "delta_acceleration 1000\n"
         << "epsilon_step_pin 1.26\n"
         << "epsilon_dir_pin 1.27\n"
         << "epsilon_en_pin nc\n"
         << "epsilon_steps_per_mm 10\n"
         << "epsilon_max_rate 6000\n"
         << "epsilon_acceleration 1000\n"
         << "acceleration 1000\n"
         << "soft_endstop.enable false\n"
         << "atc.homing_max_travel_mm 8\n"
         << "atc.homing_retract_mm 0.4\n"
         << "atc.homing_rate_mm_s 2\n"
         << "atc.detector.detect_pin 0.20^\n"
         << "atc.detector.enable true\n"
         << "coordinate.anchor1_x -359\n"
         << "coordinate.anchor1_y -234\n"
         << "coordinate.anchor2_offset_x 90\n"
         << "coordinate.anchor2_offset_y 45.65\n"
         << "coordinate.anchor_width 15\n"
         << "coordinate.rotation_offset_x -8\n"
         << "coordinate.rotation_offset_y 37.5\n"
         << "coordinate.rotation_offset_z 22.5\n"
         << "coordinate.toolrack_offset_x 356\n"
         << "coordinate.toolrack_offset_y 0\n"
         << "coordinate.toolrack_z -112.5\n"
         << "coordinate.clearance_z -3\n";
}

class TestMotionPump : public Module {
 public:
  void on_module_loaded() override { register_for_event(ON_IDLE); }
  void on_idle(void*) override { sim::pump_motion(*THEKERNEL); }
};

class CapturingStream : public StreamOutput {
 public:
  int puts(const char* buf, int size = 0) override {
    output.append(buf, size == 0 ? std::char_traits<char>::length(buf) : static_cast<std::size_t>(size));
    return size;
  }

  std::string output;
};

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_atc_test");
  const auto& root = temp_root.path();
  write_atc_config(root);
  sim::PersistentMachineState persistent_state(sim::test::persistent_sd_config(root));
  persistent_state.eeprom().reset();
  persistent_state.eeprom().configure_factory_settings({sim::MachineModel::CarveraC1, 0x04});

  sim::MachineSimulator simulator(persistent_state);
  Kernel kernel;
  kernel.eeprom_data->TOOL = 0;
  sim::attach_configured_stepper_axes(kernel);
  kernel.add_module(new TestMotionPump());
  kernel.add_module(new ATCHandler());
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();
  kernel.slow_ticker->start();

  tool_status status{};
  require(PublicData::get_value(atc_handler_checksum, get_tool_status_checksum, &status),
          "real ATCHandler should publish tool status");
  require(status.active_tool == kernel.eeprom_data->TOOL, "ATC tool status should initialize from simulated EEPROM");
  require(status.target_tool == -1, "ATC target tool should start idle");

  machine_offsets offsets{};
  require(PublicData::get_value(atc_handler_checksum, get_machine_offsets_checksum, &offsets),
          "real ATCHandler should publish machine offsets");
  require(offsets.anchor1_x == -359.0F, "ATC should load C1 anchor X from config");
  require(offsets.rotation_offset_y == 37.5F, "ATC should load C1 rotary Y offset from config");

  char pin_status[2] = {};
  require(PublicData::get_value(atc_handler_checksum, get_atc_pin_status_checksum, pin_status),
          "real ATCHandler should publish clamp endstop and detector pin states");
  require(pin_status[0] == 0 && pin_status[1] == 0, "ATC physical input pins should start inactive in the simulator");

  Gcode home_atc("M490.0", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &home_atc);
  require(!kernel.is_halted(), "C1 ATC clamp homing should not halt when the simulated home switch is reached");

  std::uint8_t clamp_status = 0;
  require(PublicData::get_value(atc_handler_checksum, get_atc_clamped_status_checksum, 0, &clamp_status),
          "real ATCHandler should publish ATC clamp status");
  require(clamp_status == 1, "M490.0 should leave the ATC clamp homed and clamped");
  require(simulator.axis_position_mm(4) > 0.0, "ATC homing should move the physical epsilon clamp axis");

  CapturingStream c1_stream;
  kernel.streams->append_stream(&c1_stream);
  Gcode c1_tool_change("M6 T1", &StreamOutput::NullStream);
  kernel.call_event(ON_GCODE_RECEIVED, &c1_tool_change);
  require(c1_stream.output.find("Start picking new tool: T1") != std::string::npos,
          "C1 with ATC should enter the rack pickup path for M6 T1");
  kernel.streams->remove_stream(&c1_stream);

  sim::PersistentMachineState air_persistent_state;
  air_persistent_state.eeprom().reset();
  air_persistent_state.eeprom().configure_factory_settings({sim::MachineModel::CarveraAirCA1, 0x00});
  sim::MachineSimulator air_simulator(air_persistent_state);
  Kernel air_kernel;
  air_kernel.eeprom_data->TOOL = 0;
  sim::attach_configured_stepper_axes(air_kernel);
  air_kernel.add_module(new ATCHandler());

  CapturingStream air_stream;
  Gcode air_tool_change("M6 T1", &air_stream);
  air_kernel.call_event(ON_GCODE_RECEIVED, &air_tool_change);
  require(air_stream.output.find("Please change the tool to: T1") != std::string::npos,
          "CA1 without the ATC function flag should use the manual tool-change path");
  return 0;
}
