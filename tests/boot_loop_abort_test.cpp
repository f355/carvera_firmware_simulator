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

#include "carvera_sim.pb.h"
#include "sim/api_service.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/system_reset.hpp"
#include "support/assertions.hpp"

namespace {

using sim::test::require;

}  // namespace

int main() {
  constexpr std::size_t kMaxSoftResets = 3;

  sim::SimulationInstance simulation;
  sim::ApiService api(simulation);
  api.set_max_boot_loop_soft_resets(kMaxSoftResets);

  carvera::sim::v1::Request request;
  request.set_id(1);
  request.mutable_start_interactive_transport()->set_enable_uart(false);
  request.mutable_start_interactive_transport()->add_tcp_ports(0);
  require(api.handle(request).ok(), "interactive transport should start");

  request.Clear();
  request.set_id(2);
  request.mutable_get_machine_snapshot();
  require(api.handle(request).ok(), "initial snapshot should boot firmware");
  require(simulation.firmware().booted(), "firmware should be booted before the boot-loop abort test");

  bool aborted = false;
  for (std::size_t i = 0; i < kMaxSoftResets + 4; ++i) {
    sim::system_reset::request();
    api.pump_interactive();
    if (simulation.firmware().boot_inhibited()) {
      aborted = true;
      break;
    }
  }

  require(aborted, "consecutive system_reset cycles should abort power-on");
  require(!simulation.firmware().booted(), "boot-loop abort should leave firmware powered off");

  request.Clear();
  request.set_id(3);
  request.mutable_get_machine_snapshot();
  require(!api.handle(request).ok(), "get_machine_snapshot should not restart an aborted boot loop");
  require(simulation.firmware().boot_inhibited(), "snapshot after abort should keep boot inhibition");
  require(!simulation.firmware().booted(), "snapshot after abort should not reconstruct Kernel");
  return 0;
}
