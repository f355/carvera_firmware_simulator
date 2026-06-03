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

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/firmware_runtime.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/machine_state_snapshot.hpp"
#include "sim/physical_scene.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void write_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "arm_solution cartesian\n"
         << "alpha_step_pin 1.28\n"
         << "alpha_dir_pin 1.29\n"
         << "alpha_en_pin nc\n"
         << "alpha_steps_per_mm 200\n"
         << "alpha_max_rate 3000\n"
         << "alpha_acceleration 150\n"
         << "alpha_min_endstop 0.24^\n"
         << "alpha_max_endstop 0.25^\n"
         << "alpha_homing_direction home_to_max\n"
         << "alpha_min 0\n"
         << "alpha_max 0\n"
         << "alpha_max_travel 20\n"
         << "alpha_limit_enable true\n"
         << "alpha_homing_retract_mm 5\n"
         << "alpha_motor_alarm_pin 0.1!^\n"
         << "beta_step_pin 1.26\n"
         << "beta_dir_pin 1.27\n"
         << "beta_en_pin nc\n"
         << "beta_steps_per_mm 200\n"
         << "beta_max_rate 3000\n"
         << "beta_acceleration 150\n"
         << "beta_min_endstop 1.1^\n"
         << "beta_max_endstop 1.4^\n"
         << "beta_homing_direction home_to_max\n"
         << "beta_min 0\n"
         << "beta_max 0\n"
         << "beta_max_travel 20\n"
         << "beta_limit_enable true\n"
         << "beta_homing_retract_mm 5\n"
         << "beta_motor_alarm_pin 0.0!^\n"
         << "gamma_step_pin 1.24\n"
         << "gamma_dir_pin 1.25\n"
         << "gamma_en_pin nc\n"
         << "gamma_steps_per_mm 200\n"
         << "gamma_max_rate 3000\n"
         << "gamma_acceleration 150\n"
         << "gamma_min_endstop 0.26^\n"
         << "gamma_max_endstop 1.8^\n"
         << "gamma_homing_direction home_to_max\n"
         << "gamma_min 0\n"
         << "gamma_max 0\n"
         << "gamma_max_travel 20\n"
         << "gamma_limit_enable true\n"
         << "gamma_homing_retract_mm 5\n"
         << "gamma_motor_alarm_pin 3.25!^\n"
         << "delta_step_pin 1.18\n"
         << "delta_dir_pin 1.20!\n"
         << "delta_en_pin nc\n"
         << "delta_steps_per_mm 26.666667\n"
         << "delta_max_rate 1800\n"
         << "delta_acceleration 360\n"
         << "delta_min_endstop 0.21!^\n"
         << "delta_max_endstop 0.21!^\n"
         << "delta_homing_direction home_to_min\n"
         << "delta_min -0.5\n"
         << "delta_max 0\n"
         << "delta_max_travel 380\n"
         << "delta_limit_enable true\n"
         << "delta_homing_retract_mm 0.5\n"
         << "e_stop_pin 2.15!^\n"
         << "sd_ok true\n"
         << "endstop_debounce_ms 0\n"
         << "soft_endstop.enable true\n"
         << "soft_endstop.x_min -10\n"
         << "soft_endstop.y_min -20\n"
         << "soft_endstop.z_min -30\n";
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_machine_state_snapshot_test");
  const auto& root = temp_root.path();
  write_config(root);
  sim::host_filesystem::clear_mounts();
  sim::host_filesystem::mount("sd", root);

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime firmware(simulator);
  sim::physical_scene::active().set_atc_pocket_tool(1, 1, true, 62.0, sim::ToolKind::CuttingTool);
  sim::physical_scene::active().set_spindle_tool(3, 55.5, true, sim::ToolKind::ThreeAxisProbe, 2.5);

  auto& kernel = firmware.boot();
  require(kernel.robot != nullptr, "test firmware should boot Robot");

  const auto state =
      sim::assemble_machine_state(kernel, firmware.is_homed(), firmware.factory_settings().machine_model);
  require(state.firmware_booted, "shared machine state should report booted firmware");
  require(state.homed, "shared machine state should report boot homing completion");
  require(state.soft_endstop_enabled, "shared machine state should expose firmware soft-limit state");
  require(state.work_area.has_value(), "shared machine state should expose firmware work area");
  require(state.work_area->min_x == -10.0, "shared machine state should preserve firmware soft-limit X min");
  require(state.work_area->max_x == -1.0, "shared machine state should preserve firmware soft-limit X max");
  require(state.physical_travel.has_value(), "shared machine state should expose simulator physical travel");
  require(state.physical_travel->min_x == -372.0, "shared machine state should expose C1 physical X travel");
  require(state.physical_travel->max_z == 1.0, "shared machine state should expose C1 physical Z travel");
  require(state.tool_setter.has_value(), "shared machine state should expose physical tool-setter geometry");
  require(state.axes.size() >= 5, "shared machine state should include firmware-configured axes");
  require(state.axes[0].axis == 0, "shared machine state axis zero should be X");
  require(state.axes[3].axis == 3, "shared machine state axis three should be A");
  require(state.axes[4].axis == 4, "shared machine state axis four should be ATC/B");
  require(state.axes[1].physical_mm < -3.9, "shared machine state should include stepper-derived physical Y");
  require(state.atc.available, "shared machine state should expose ATC availability");
  require(state.atc.pockets.size() == 1, "shared machine state should expose physical ATC pocket tools");
  require(state.atc.pockets[0].length_mm == 62.0, "shared machine state should preserve pocket tool length");
  require(state.atc.spindle.has_tool, "shared machine state should expose simulated spindle tool");
  require(state.atc.spindle.kind == sim::ToolKind::ThreeAxisProbe, "shared machine state should preserve tool kind");
  require(state.atc.spindle.probe_tip_diameter_mm == 2.5, "shared machine state should preserve probe tip diameter");
  return 0;
}
