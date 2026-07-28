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

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Robot.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/motion_telemetry.hpp"
#include "sim/simulation_instance.hpp"
#include "sim/simulator_context.hpp"
#include "support/assertions.hpp"
#include "support/temp_sdcard.hpp"

using sim::test::require;

namespace {

std::vector<std::int64_t> physical_steps(sim::SimulationInstance& simulation) {
  std::vector<std::int64_t> steps;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    steps.push_back(simulation.machine().axis_position_steps(axis));
  }
  return steps;
}

}  // namespace

// Motion telemetry is emitted on an interval, so the final in-motion sample
// lands a few steps before the move finishes. The resting position must still
// reach the sink, otherwise GUI axis readouts freeze a few microns short of
// where the machine actually stopped.
int main() {
  sim::test::TempSdCard sd("carvera_sim_motion_telemetry_resting_position_test");
  sd.write_config_txt("protocol makera\nsd_ok true\nsoft_endstop.enable true\n");
  sim::SimulationInstance simulation(sd.persistent_config());

  std::vector<std::int64_t> last_emitted;
  simulation.machine().context().motion_telemetry().set_sink([&](const sim::MachineTelemetry& sample) {
    last_emitted.clear();
    for (const auto& axis : sample.axes) {
      if (last_emitted.size() < 3) {
        last_emitted.push_back(axis.physical_steps);
      }
    }
  });

  auto& runtime = simulation.firmware();
  runtime.boot();
  require(runtime.is_homed(), "boot homing should complete before the telemetry check");

  // Widen the emit interval so the whole probe move fits inside one window.
  // That is the case the GUI hits at the end of every homing backoff: the last
  // in-motion sample is already spent, and the axes come to rest before the
  // next interval is due.
  auto& telemetry = simulation.machine().context().motion_telemetry();
  telemetry.set_interval_ticks(1'000'000);
  telemetry.set_interval_us(200'000);

  const auto before = physical_steps(simulation);

  // Interleave main-loop iterations so the firmware keeps feeding its watchdog.
  runtime.io().write_wifi_command("G91\nG0 X-0.35 Y-0.21 F900\nG90\n");
  bool idle = false;
  for (int i = 0; i < 20'000 && !idle; ++i) {
    idle = runtime.runner().pump_free_running(4, 4'000) && i > 8;
  }
  require(idle, "the telemetry probe move should reach motion idle");

  const auto resting = physical_steps(simulation);
  require(resting != before, "the telemetry probe move should actually have moved the axes");

  // Idle for well over the emit interval, so a correct implementation has had
  // every opportunity to publish the resting position.
  for (int i = 0; i < 4'000; ++i) {
    runtime.runner().pump_free_running(4, 4'000);
  }

  require(!last_emitted.empty(), "telemetry sink should have received at least one sample");
  if (last_emitted != resting) {
    std::fprintf(stderr, "resting=[%lld %lld %lld] last emitted=[%lld %lld %lld]\n",
                 static_cast<long long>(resting[0]), static_cast<long long>(resting[1]),
                 static_cast<long long>(resting[2]), static_cast<long long>(last_emitted[0]),
                 static_cast<long long>(last_emitted[1]), static_cast<long long>(last_emitted[2]));
    require(false, "last emitted telemetry sample must carry the resting physical position, not a mid-move sample");
  }
  return 0;
}
