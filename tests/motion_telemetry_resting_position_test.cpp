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
#include <algorithm>
#include <cmath>
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
  bool saw_physical_motion = false;
  double last_x_speed = -1.0;
  std::vector<double> observed_x_speeds;
  simulation.machine().context().motion_telemetry().set_sink([&](const sim::MachineTelemetry& sample) {
    last_emitted.clear();
    for (const auto& axis : sample.axes) {
      if (last_emitted.size() < 3) {
        last_emitted.push_back(axis.physical_steps);
      }
      if (axis.axis == 0) {
        last_x_speed = axis.physical_speed_per_min;
        saw_physical_motion = saw_physical_motion || axis.physical_speed_per_min > 0.0;
        if (axis.physical_speed_per_min > 0.0) {
          observed_x_speeds.push_back(axis.physical_speed_per_min);
        }
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
  constexpr std::uint64_t emit_interval_us = 100'000;
  auto& telemetry = simulation.machine().context().motion_telemetry();
  telemetry.set_interval_ticks(1'000'000);
  telemetry.set_interval_us(emit_interval_us);

  const auto before = physical_steps(simulation);

  // Short enough to finish well inside one emit window. Interleave main-loop
  // iterations so the firmware keeps feeding its watchdog.
  observed_x_speeds.clear();
  saw_physical_motion = false;
  runtime.io().write_wifi_command("G91\nG0 X-0.04 Y-0.03 F900\nG90\n");
  bool idle = false;
  for (int i = 0; i < 20'000 && !idle; ++i) {
    idle = runtime.runner().pump_free_running(4, 2'000) && i > 8;
  }
  require(idle, "the telemetry probe move should reach motion idle");

  const auto resting = physical_steps(simulation);
  require(resting != before, "the telemetry probe move should actually have moved the axes");

  // Idle for several emit intervals of simulated time, so a correct
  // implementation has had every opportunity to publish the resting position.
  // Bounded by simulated time rather than an iteration count so the runtime
  // does not depend on host speed.
  const auto idle_started_us = simulation.machine().time_us();
  while (last_emitted != resting && simulation.machine().time_us() - idle_started_us < 10 * emit_interval_us) {
    runtime.runner().pump_free_running(4, 2'000);
  }

  require(!last_emitted.empty(), "telemetry sink should have received at least one sample");
  if (last_emitted != resting) {
    std::fprintf(stderr, "resting=[%lld %lld %lld] last emitted=[%lld %lld %lld]\n", static_cast<long long>(resting[0]),
                 static_cast<long long>(resting[1]), static_cast<long long>(resting[2]),
                 static_cast<long long>(last_emitted[0]), static_cast<long long>(last_emitted[1]),
                 static_cast<long long>(last_emitted[2]));
    require(false, "last emitted telemetry sample must carry the resting physical position, not a mid-move sample");
  }

  // Exercise enough motion to span several telemetry samples. The reported
  // speed is derived from physical steps, and the subsequent resting sample
  // must clear it instead of leaving the last moving value on screen.
  telemetry.set_interval_ticks(1);
  telemetry.set_interval_us(1'000);
  observed_x_speeds.clear();
  saw_physical_motion = false;
  runtime.io().write_wifi_command("G91\nG0 X0.04 F900\nG90\n");
  idle = false;
  for (int i = 0; i < 20'000 && !idle; ++i) {
    idle = runtime.runner().pump_free_running(4, 2'000) && i > 8;
  }
  require(idle, "the physical-speed spike probe should reach motion idle");
  require(*std::max_element(observed_x_speeds.begin(), observed_x_speeds.end()) < 100.0,
          "the physical-speed window should suppress individual step timing spikes");

  observed_x_speeds.clear();
  saw_physical_motion = false;
  runtime.io().write_wifi_command("G91\nG1 X-0.5 F60\nG90\n");
  idle = false;
  for (int i = 0; i < 20'000 && !idle; ++i) {
    idle = runtime.runner().pump_free_running(4, 2'000) && i > 8;
  }
  require(idle, "the physical-speed probe move should reach motion idle");
  const auto speed_idle_started_us = simulation.machine().time_us();
  while (last_x_speed != 0.0 && simulation.machine().time_us() - speed_idle_started_us < 10 * emit_interval_us) {
    runtime.runner().pump_free_running(4, 2'000);
  }
  require(saw_physical_motion, "moving physical steps should produce a positive physical speed");
  require(*std::max_element(observed_x_speeds.begin(), observed_x_speeds.end()) < 90.0,
          "a 100 ms physical-speed window should suppress individual step timing spikes");
  require(std::fabs(last_x_speed) < 1e-9, "resting physical axes should report zero speed");
  return 0;
}
