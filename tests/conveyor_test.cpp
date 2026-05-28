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

#include <cstddef>
#include <cstdint>

#define private public
#define protected public
#include "Conveyor.h"
#undef protected
#undef private

#include <cstdlib>
#include <iostream>

#include "Block.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.conveyor->start(1);
  require(kernel.conveyor->queue.length == 32, "Conveyor should allocate the configured default queue length");
  require(kernel.conveyor->is_queue_empty(), "new Conveyor queue should start empty");
  kernel.conveyor->on_idle(nullptr);

  Block* queued = kernel.conveyor->queue.head_ref();
  queued->steps[0] = 1;
  queued->steps_event_count = 1;
  queued->nominal_speed = 25.0F;
  queued->ready();
  kernel.conveyor->queue_head_block();

  Block* next = nullptr;
  require(!kernel.conveyor->get_next_block(&next), "Conveyor should hold a fresh block until queue delay expires");

  simulator.advance_us(100'000);
  kernel.conveyor->on_idle(nullptr);

  require(kernel.conveyor->get_next_block(&next), "Conveyor should expose a queued block after the queue delay");
  require(next == queued, "Conveyor should return the queued head block");
  require(next->is_ticking, "Conveyor should mark returned blocks as ticking");
  require(kernel.conveyor->get_current_feedrate() == 25.0F, "Conveyor should report current block feedrate");

  kernel.conveyor->block_finished();
  require(kernel.conveyor->queue.tail_i != kernel.conveyor->queue.isr_tail_i,
          "Conveyor should leave finished ISR blocks for idle cleanup");
  kernel.conveyor->on_idle(nullptr);
  require(kernel.conveyor->is_queue_empty(), "Conveyor idle handler should reclaim finished blocks");

  return 0;
}
