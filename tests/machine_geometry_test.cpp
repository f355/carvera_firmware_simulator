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

#include "sim/i2c_eeprom.hpp"
#include "sim/machine_geometry.hpp"
#include "support/assertions.hpp"

using sim::test::require;
using sim::test::require_equal;

int main() {
  const auto c1 = sim::geometry_for(sim::MachineModel::CarveraC1);
  require(c1.has_value(), "C1 should expose simulator-owned hardware geometry");
  require_equal(c1->physical_travel.min_x, -372.0, "C1 physical X min should sit outside the stock soft limit");
  require_equal(c1->physical_travel.max_x, 1.0, "C1 physical X max should sit outside the stock soft limit");
  require_equal(c1->physical_travel.min_y, -251.0, "C1 physical Y min should sit outside the stock soft limit");
  require_equal(c1->physical_travel.max_y, 1.0, "C1 physical Y max should sit outside the stock soft limit");
  require_equal(c1->physical_travel.min_z, -136.0, "C1 physical Z min should sit outside the stock soft limit");
  require_equal(c1->physical_travel.max_z, 1.0, "C1 physical Z max should sit outside the stock soft limit");
  require_equal(c1->bed_z, -147.5, "C1 bed should sit below the spindle-face travel limit");
  require_equal(c1->tool_setter.max_z, -105.5,
                "C1 ETS trigger point should sit 1mm below an 8mm button above the tool rack");
  require_equal(c1->tool_setter.min_z, c1->tool_setter.max_z - 2.0,
                "C1 ETS contact volume should keep a small finite thickness");

  const auto ca1 = sim::geometry_for(sim::MachineModel::CarveraAirCA1);
  require(ca1.has_value(), "CA1 should have simulator-owned hardware geometry");
  require_equal(ca1->physical_travel.min_x, -303.0, "CA1 physical X min should be hardware-owned");
  require_equal(ca1->physical_travel.max_x, 1.0, "CA1 physical X max should be hardware-owned");
  require_equal(ca1->physical_travel.min_y, -213.0, "CA1 physical Y min should be hardware-owned");
  require_equal(ca1->physical_travel.max_y, 1.0, "CA1 physical Y max should be hardware-owned");
  require_equal(ca1->physical_travel.min_z, -122.0, "CA1 physical Z min should be hardware-owned");
  require_equal(ca1->physical_travel.max_z, 1.0, "CA1 physical Z max should be hardware-owned");
  require_equal(ca1->bed_z, -137.0, "CA1 bed should sit below the spindle-face travel limit");
  require_equal(ca1->tool_setter.min_x, -17.0, "CA1 ETS X min should be fixed physical geometry");
  require_equal(ca1->tool_setter.max_x, -5.0, "CA1 ETS X max should be fixed physical geometry");
  require_equal(ca1->tool_setter.min_y, -13.0, "CA1 ETS Y min should be fixed physical geometry");
  require_equal(ca1->tool_setter.max_y, -1.0, "CA1 ETS Y max should be fixed physical geometry");
  require_equal(ca1->tool_setter.max_z, -115.0, "CA1 ETS trigger point should match the modeled ETS top");
  require_equal(ca1->tool_setter.min_z, ca1->tool_setter.max_z - 2.0,
                "CA1 ETS contact volume should keep a small finite thickness");

  return 0;
}
