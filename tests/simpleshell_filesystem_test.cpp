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

#include <filesystem>
#include <fstream>
#include <string>

#include "sim/firmware_runtime.hpp"
#include "sim/simulation_instance.hpp"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::require;
using sim::test::require_contains;

std::string run_shell_command(sim::FirmwareRuntime& runtime, const std::string& command) {
  runtime.io().write_serial(command + "\n");
  runtime.runner().run_main_loop(16);
  return runtime.io().read_serial();
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_simpleshell_filesystem_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.sd_ok = false;
  sim::test::write_cartesian_config(root, config);
  std::filesystem::create_directories(root / "jobs");
  {
    std::ofstream gcode(root / "jobs" / "demo.cnc");
    gcode << "G90\nG0 X1\nM5\n";
  }

  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  runtime.boot();
  runtime.io().read_serial();

  require_contains(run_shell_command(runtime, "pwd"), "/", "SimpleShell should start at the filesystem root");
  require_contains(run_shell_command(runtime, "ls /sd/jobs"), "demo.cnc",
                   "SimpleShell should list files from the mounted SD card");
  require_contains(run_shell_command(runtime, "cat /sd/jobs/demo.cnc"), "G90\nG0 X1\nM5\n",
                   "SimpleShell should return complete G-code file contents");

  const auto md5_output = run_shell_command(runtime, "md5sum /sd/jobs/demo.cnc");
  require_contains(md5_output, "a7fbe2defab7f89f0a81a8972fc37efe",
                   "SimpleShell should calculate the uploaded file's MD5 digest");
  require_contains(md5_output, "/sd/jobs/demo.cnc", "SimpleShell MD5 output should identify its source file");

  require_contains(run_shell_command(runtime, "mkdir /sd/jobs/archive"), "created directory /sd/jobs/archive",
                   "SimpleShell should create directories on the SD card");
  require(std::filesystem::is_directory(root / "jobs" / "archive"),
          "SimpleShell mkdir should affect the mounted host filesystem");

  run_shell_command(runtime, "cd /sd/jobs");
  require_contains(run_shell_command(runtime, "pwd"), "/sd/jobs",
                   "SimpleShell should retain the current working directory");
  require_contains(run_shell_command(runtime, "mv demo.cnc renamed.cnc"),
                   "renamed /sd/jobs/demo.cnc to /sd/jobs/renamed.cnc",
                   "SimpleShell should rename files relative to its working directory");
  require(!std::filesystem::exists(root / "jobs" / "demo.cnc") &&
              std::filesystem::exists(root / "jobs" / "renamed.cnc"),
          "SimpleShell mv should rename the mounted host file");
  require_contains(run_shell_command(runtime, "ls"), "renamed.cnc",
                   "SimpleShell relative directory listing should use its working directory");

  run_shell_command(runtime, "rm renamed.cnc");
  require(!std::filesystem::exists(root / "jobs" / "renamed.cnc"),
          "SimpleShell rm should delete files from the mounted SD card");
  require_contains(run_shell_command(runtime, "cat renamed.cnc"), "File not found: /sd/jobs/renamed.cnc",
                   "SimpleShell should report a missing file after deletion");

  require_contains(run_shell_command(runtime, "cd missing"), "Could not open directory /sd/jobs/missing",
                   "SimpleShell should reject a missing working directory");
  require_contains(run_shell_command(runtime, "echo filesystem ready"), "echo: filesystem ready",
                   "SimpleShell should echo controller diagnostics to its streams");
  require_contains(run_shell_command(runtime, "help"), "md5sum file",
                   "SimpleShell help should advertise file integrity commands available on real machines");

  return 0;
}
