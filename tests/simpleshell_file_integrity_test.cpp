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

// M576 walks the SD card through fwfs opendir/readdir, which the simulator
// backs with the host filesystem rather than ChaNFS. Exercise the firmware's
// own integrity checks end to end so that divergence in the host filesystem
// shim shows up here instead of as a confusing firmware-side failure.

#include <filesystem>
#include <fstream>
#include <string>

#include "sim/firmware_runtime.hpp"
#include "sim/simulation_instance.hpp"
#include "support/assertions.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

using sim::test::require_contains;

std::string run_shell_command(sim::FirmwareRuntime& runtime, const std::string& command) {
  runtime.io().write_serial_command(command + "\n");
  runtime.runner().run_main_loop(32);
  return runtime.io().read_serial_text();
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << contents;
}

}  // namespace

int main() {
  sim::test::TempDirectory temp_root("carvera_sim_simpleshell_file_integrity_test");
  const auto& root = temp_root.path();
  sim::test::CartesianConfigOptions config;
  config.sd_ok = false;
  sim::test::write_cartesian_config(root, config);

  // "good.cnc" keeps the digest the firmware will recompute; "bad.cnc" has a
  // stale sidecar, standing in for a file corrupted after upload.
  write_file(root / "gcodes" / "good.cnc", "G90\nG0 X1\nM5\n");
  write_file(root / "gcodes" / ".md5" / "good.cnc", "a7fbe2defab7f89f0a81a8972fc37efe");
  write_file(root / "gcodes" / "bad.cnc", "G90\nG0 X2\nM5\n");
  write_file(root / "gcodes" / ".md5" / "bad.cnc", "a7fbe2defab7f89f0a81a8972fc37efe");
  write_file(root / "gcodes" / "unhashed.cnc", "G90\n");

  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  runtime.boot();
  runtime.io().read_serial_text();

  // The stored digest has to match what the firmware computes for the same
  // bytes, otherwise "Intact" below would only prove both sides agree on being
  // wrong.
  require_contains(run_shell_command(runtime, "md5sum /sd/gcodes/good.cnc"), "a7fbe2defab7f89f0a81a8972fc37efe",
                   "the fixture digest should match the firmware's own MD5 of the file");

  require_contains(run_shell_command(runtime, "M576.2 /sd/gcodes/good.cnc"), "Intact",
                   "M576.2 should verify a single file against its stored digest");
  require_contains(run_shell_command(runtime, "M576.2 /sd/gcodes/bad.cnc"), "Corrupt",
                   "M576.2 should report a file whose contents no longer match its digest");
  require_contains(run_shell_command(runtime, "M576.2 /sd/gcodes/unhashed.cnc"), "No MD5 hash found",
                   "M576.2 should say so when a file has no stored digest");
  require_contains(run_shell_command(runtime, "M576.2 /etc/passwd"), "Path must be under /sd/gcodes/",
                   "M576.2 should refuse paths outside the gcode directory");

  // The recursive walk has to traverse the directory through the simulator's
  // host-backed opendir/readdir and reach both hashed files.
  const auto directory_walk = run_shell_command(runtime, "M576.2 /sd/gcodes");
  require_contains(directory_walk, "1 intact", "the directory walk should verify the intact file");
  require_contains(directory_walk, "1 corrupt", "the directory walk should flag the corrupt file");

  const auto full_walk = run_shell_command(runtime, "M576.1");
  require_contains(full_walk, "1 intact", "M576.1 should walk /sd/gcodes and verify the intact file");
  require_contains(full_walk, "1 corrupt", "M576.1 should walk /sd/gcodes and flag the corrupt file");

  return 0;
}
