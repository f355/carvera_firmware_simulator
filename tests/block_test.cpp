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
#include <iostream>

#include "Block.h"
#include "libs/Kernel.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  Kernel kernel;

  Block::init(1);

  Block block;
  block.steps[0] = 1000;
  block.steps_event_count = 1000;
  block.nominal_rate = 5000.0F;
  block.nominal_speed = 50.0F;
  block.millimeters = 10.0F;
  block.acceleration = 100.0F;
  block.max_entry_speed = 50.0F;

  block.calculate_trapezoid(10.0F, 10.0F);

  require(block.tick_info[0].steps_to_move == 1000, "Block should prepare per-motor steps for StepTicker");
  require(block.total_move_ticks > 0, "Block should calculate a non-zero move duration");
  require(block.decelerate_after <= block.total_move_ticks, "Block deceleration point should be within move duration");
  require(block.get_trapezoid_rate(0) > 0.0F, "Block should expose the prepared StepTicker rate");
  require(!block.locked, "Block should unlock after trapezoid preparation");

  return 0;
}
