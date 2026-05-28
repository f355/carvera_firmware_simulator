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

#ifndef SIMULATOR_SIM_ATC_RACK_MODEL_HPP
#define SIMULATOR_SIM_ATC_RACK_MODEL_HPP

#include <optional>
#include <vector>

#include "sim/physical_scene_types.hpp"

namespace sim {

class AtcRackModel {
 public:
  void clear();

  void set_pocket_tool(int pocket, int tool, bool occupied, double length_mm, ToolKind kind = ToolKind::Unspecified,
                       double probe_tip_diameter_mm = 0.0);
  void clear_pocket_tools();
  std::vector<AtcPocketState> pockets() const;

  void configure(const PhysicalAtcConfig& config);
  bool available() const { return available_; }

  bool update_clamp_position(Point3 spindle_position, double clamp_position_mm);
  void clamp_open(Point3 spindle_position);
  void clamp_close(Point3 spindle_position);

  void set_spindle_tool(int tool, double length_mm, bool installed, ToolKind kind = ToolKind::Unspecified,
                        double probe_tip_diameter_mm = 0.0);
  AtcSpindleState spindle() const { return spindle_tool_; }
  bool spindle_has_probe_tool() const;
  bool detector_contact(Point3 spindle_position) const;

 private:
  void reconcile_spindle_tool_from_firmware(int active_tool);

  std::vector<AtcPocketState> pockets_;
  AtcSpindleState spindle_tool_;
  std::optional<double> last_clamp_position_mm_;
  std::optional<double> clamp_reference_position_mm_;
  double clamp_action_mm_{1.0};
  bool physical_clamped_{true};
  bool available_{false};
};

}  // namespace sim

#endif
