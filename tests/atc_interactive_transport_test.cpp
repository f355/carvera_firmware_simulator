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

#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>

#include "sim/delay_hooks.hpp"
#include "sim/interactive_io.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/physical_scene.hpp"
#include "support/assertions.hpp"
#include "support/c1_atc_config.hpp"
#include "support/posix_io.hpp"
#include "support/temp_sdcard.hpp"

#include <unistd.h>

namespace {

using sim::test::require;

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_atc_interactive_transport_test");
  sim::test::C1AtcConfigOptions config_options;
  config_options.include_atc_home_pin = false;
  sim::test::write_c1_atc_config(sd.path(), config_options);
  sim::SimulationInstance simulation(sd.persistent_config());
  sim::physical_scene::active().set_atc_pocket_tool(2, 2, true, 62.0);
  auto& runtime = simulation.firmware();
  runtime.boot();
  sim::LocalhostTcpBridge bridge(runtime);
  require(bridge.start(0), "localhost WiFi bridge should start");

  auto pump_once = [&] {
    bridge.poll();
    {
      sim::delay_hooks::ScopedCallback blocking_wait_io_pump([&] { bridge.poll(); });
      runtime.pump_free_running(8, 100'000);
    }
    bridge.poll();
  };

  int client = -1;
  require(sim::test::connect_loopback(bridge.port(), client),
          "controller client should connect to localhost WiFi bridge");

  const char initial_status_poll[] = "?\n";
  require(sim::test::write_exact(client, initial_status_poll, std::strlen(initial_status_poll)),
          "initial status poll should write");
  const auto initial_status = sim::test::read_until_pumping(client, "MPos:", pump_once, std::chrono::seconds(5));
  require(initial_status.find("<Idle") != std::string::npos, "initial controller status query should return Idle");

  const char tool_change[] = "M6 T2\n";
  require(sim::test::write_exact(client, tool_change, std::strlen(tool_change)),
          "ATC tool-change command should write");
  const auto atc_started = sim::test::read_until_pumping(client, "Homing atc", pump_once, std::chrono::seconds(8));
  if (atc_started.find("Homing atc") == std::string::npos) {
    std::cerr << atc_started << '\n';
  }
  require(atc_started.find("Homing atc") != std::string::npos, "C1 ATC should enter the clamp-homing phase");

  const char status_poll[] = "?\n";
  require(sim::test::write_exact(client, status_poll, std::strlen(status_poll)), "status poll during ATC should write");
  const auto status = sim::test::read_until_pumping(client, "MPos:", pump_once, std::chrono::seconds(3));
  require(status.find("MPos:") != std::string::npos, "interactive transport should answer status polls during ATC");

  ::close(client);
  return 0;
}
