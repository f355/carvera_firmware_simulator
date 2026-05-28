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

#ifndef SIMULATOR_SIM_PROBE_CONTACT_MODEL_HPP
#define SIMULATOR_SIM_PROBE_CONTACT_MODEL_HPP

#include <optional>

#include "sim/physical_scene_types.hpp"

namespace sim {

class ProbeContactModel {
 public:
  void clear();

  void set_probe_tool_installed(bool installed) { probe_tool_installed_ = installed; }
  bool probe_tool_installed() const { return probe_tool_installed_; }

  void set_tool_setter_box(const Box& box);
  void clear_tool_setter_box();
  void configure_tool_setter(std::optional<Box> box);
  std::optional<Box> tool_setter_box() const { return tool_setter_box_; }

  void set_stock_box(const Box& box);
  void clear_stock_box() { stock_box_.reset(); }

  ProbeContactState update(Point3 spindle_probe_position, const AtcSpindleState& spindle_tool);

 private:
  bool probe_tool_installed_{false};
  std::optional<Box> tool_setter_box_;
  std::optional<Box> stock_box_;
  std::optional<Point3> previous_probe_position_;
  bool tool_setter_overridden_{false};
};

}  // namespace sim

#endif
