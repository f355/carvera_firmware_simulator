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

#ifndef SIMULATOR_SIM_PHYSICAL_SCENE_HPP
#define SIMULATOR_SIM_PHYSICAL_SCENE_HPP

#include <cstddef>
#include <optional>
#include <vector>

#include "sim/atc_rack_model.hpp"
#include "sim/i2c_eeprom.hpp"
#include "sim/physical_scene_types.hpp"
#include "sim/physical_signal_driver.hpp"
#include "sim/probe_contact_model.hpp"

namespace sim {

class PhysicalScene {
 public:
  void clear();

  void set_probe_tool_installed(bool installed) { probe_contacts_.set_probe_tool_installed(installed); }
  bool probe_tool_installed() const;

  void set_tool_setter_box(const Box& box) { probe_contacts_.set_tool_setter_box(box); }
  void clear_tool_setter_box() { probe_contacts_.clear_tool_setter_box(); }

  void set_stock_box(const Box& box) { probe_contacts_.set_stock_box(box); }
  void clear_stock_box() { probe_contacts_.clear_stock_box(); }

  void set_atc_pocket_tool(int pocket, int tool, bool occupied, double length_mm, ToolKind kind = ToolKind::Unspecified,
                           double probe_tip_diameter_mm = 0.0);
  void clear_atc_pocket_tools();
  void configure_atc(const PhysicalAtcConfig& config);
  void update_atc_clamp_position(Point3 spindle_position, double clamp_position_mm);
  void atc_clamp_open(Point3 spindle_position);
  void atc_clamp_close(Point3 spindle_position);
  void set_spindle_tool(int tool, double length_mm, bool installed, ToolKind kind = ToolKind::Unspecified,
                        double probe_tip_diameter_mm = 0.0);
  std::vector<AtcPocketState> atc_pockets() const { return atc_rack_.pockets(); }
  AtcSpindleState atc_spindle() const { return atc_rack_.spindle(); }
  std::optional<Box> tool_setter_box() const { return probe_contacts_.tool_setter_box(); }
  bool atc_available() const { return atc_rack_.available(); }
  std::optional<std::size_t> stock_probe_crash_axis() const { return last_contacts_.stock_probe_crash_axis; }

  void update_probe_contacts(Point3 spindle_probe_position);

 private:
  void sync_probe_install_from_spindle();

  MachineModel model_{MachineModel::CarveraC1};
  ProbeContactModel probe_contacts_;
  AtcRackModel atc_rack_;
  PhysicalSignalDriver signal_driver_;
  ProbeContactState last_contacts_;
};

namespace physical_scene {

PhysicalScene& active();
void reset();

}  // namespace physical_scene

}  // namespace sim

#endif
