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

#include "sim/atc_rack_model.hpp"

#include <algorithm>
#include <cmath>

#include "sim/physical_tooling.hpp"

namespace sim {
namespace {

constexpr double tool_transfer_z_tolerance_mm = 3.0;

bool at_rack_transfer_position(Point3 spindle_position, const AtcPocketState& pocket) {
  using physical_tooling::close_enough;
  return close_enough(spindle_position.x, pocket.position.x, physical_tooling::detector_tolerance_mm) &&
         close_enough(spindle_position.y, pocket.position.y, physical_tooling::detector_tolerance_mm) &&
         close_enough(spindle_position.z, pocket.position.z, tool_transfer_z_tolerance_mm);
}

}  // namespace

void AtcRackModel::clear() {
  pockets_.clear();
  spindle_tool_ = {};
  last_clamp_position_mm_.reset();
  clamp_reference_position_mm_.reset();
  clamp_action_mm_ = 1.0;
  physical_clamped_ = true;
  available_ = false;
}

void AtcRackModel::set_pocket_tool(int pocket, int tool, bool occupied, double length_mm, ToolKind kind,
                                   double probe_tip_diameter_mm) {
  const ToolKind resolved_kind = physical_tooling::resolve_tool_kind(tool, kind);
  const double resolved_probe_tip_diameter =
      physical_tooling::resolve_probe_tip_diameter(tool, resolved_kind, probe_tip_diameter_mm);
  auto existing = std::find_if(pockets_.begin(), pockets_.end(),
                               [pocket](const AtcPocketState& state) { return state.pocket == pocket; });
  if (existing == pockets_.end()) {
    pockets_.push_back(AtcPocketState{
        pocket,
        tool,
        occupied,
        length_mm,
        resolved_kind,
        resolved_probe_tip_diameter,
        {},
    });
    return;
  }

  existing->tool = tool;
  existing->occupied = occupied;
  existing->length_mm = length_mm;
  existing->kind = resolved_kind;
  existing->probe_tip_diameter_mm = resolved_probe_tip_diameter;
}

void AtcRackModel::clear_pocket_tools() {
  pockets_.clear();
  spindle_tool_ = {};
}

std::vector<AtcPocketState> AtcRackModel::pockets() const {
  auto pockets = pockets_;
  std::sort(pockets.begin(), pockets.end(),
            [](const AtcPocketState& lhs, const AtcPocketState& rhs) { return lhs.pocket < rhs.pocket; });
  return pockets;
}

void AtcRackModel::configure(const PhysicalAtcConfig& config) {
  if (config.persisted_active_tool.has_value()) {
    reconcile_spindle_tool_from_firmware(*config.persisted_active_tool);
  }

  if (config.model != MachineModel::CarveraC1 || !config.atc_available) {
    available_ = false;
    return;
  }

  clamp_action_mm_ = config.clamp_action_mm;
  for (auto& pocket : pockets_) {
    pocket.position = Point3{
        config.anchor1_x + config.toolrack_offset_x,
        config.anchor1_y + config.toolrack_offset_y + (pocket.pocket == 0 ? 210.0 : (6 - pocket.pocket) * 30.0),
        config.toolrack_z,
    };
  }
  available_ = true;
}

bool AtcRackModel::update_clamp_position(Point3 spindle_position, double clamp_position_mm) {
  const auto previous_position = last_clamp_position_mm_;
  last_clamp_position_mm_ = clamp_position_mm;
  if (!previous_position.has_value()) {
    clamp_reference_position_mm_ = clamp_position_mm;
    return false;
  }

  if (!clamp_reference_position_mm_.has_value()) {
    clamp_reference_position_mm_ = clamp_position_mm;
  }

  const double threshold = std::max(0.05, std::abs(clamp_action_mm_) * 0.8);
  const double delta = clamp_position_mm - *previous_position;
  if (physical_clamped_ && delta > 0.0) {
    clamp_reference_position_mm_ = std::max(*clamp_reference_position_mm_, clamp_position_mm);
    return false;
  }
  if (!physical_clamped_ && delta < 0.0) {
    clamp_reference_position_mm_ = std::min(*clamp_reference_position_mm_, clamp_position_mm);
    return false;
  }

  if (physical_clamped_ && delta < 0.0 && *clamp_reference_position_mm_ - clamp_position_mm >= threshold) {
    physical_clamped_ = false;
    clamp_reference_position_mm_ = clamp_position_mm;
    clamp_open(spindle_position);
    return true;
  } else if (!physical_clamped_ && delta > 0.0 && clamp_position_mm - *clamp_reference_position_mm_ >= threshold) {
    physical_clamped_ = true;
    clamp_reference_position_mm_ = clamp_position_mm;
    clamp_close(spindle_position);
    return true;
  }
  return false;
}

void AtcRackModel::clamp_open(Point3 spindle_position) {
  if (!spindle_tool_.has_tool) {
    return;
  }
  for (auto& pocket : pockets_) {
    if (at_rack_transfer_position(spindle_position, pocket)) {
      pocket.tool = spindle_tool_.tool;
      pocket.length_mm = spindle_tool_.length_mm;
      pocket.kind = spindle_tool_.kind;
      pocket.probe_tip_diameter_mm = spindle_tool_.probe_tip_diameter_mm;
      pocket.occupied = true;
      spindle_tool_ = {};
      return;
    }
  }
}

void AtcRackModel::clamp_close(Point3 spindle_position) {
  if (spindle_tool_.has_tool) {
    return;
  }
  for (auto& pocket : pockets_) {
    if (!pocket.occupied) {
      continue;
    }
    if (at_rack_transfer_position(spindle_position, pocket)) {
      spindle_tool_ = AtcSpindleState{pocket.tool, pocket.length_mm, pocket.kind, pocket.probe_tip_diameter_mm, true};
      pocket.occupied = false;
      return;
    }
  }
}

void AtcRackModel::set_spindle_tool(int tool, double length_mm, bool installed, ToolKind kind,
                                    double probe_tip_diameter_mm) {
  if (!installed) {
    spindle_tool_ = {};
    return;
  }
  const ToolKind resolved_kind = physical_tooling::resolve_tool_kind(tool, kind);
  spindle_tool_ = AtcSpindleState{
      tool,          length_mm,
      resolved_kind, physical_tooling::resolve_probe_tip_diameter(tool, resolved_kind, probe_tip_diameter_mm),
      true,
  };
}

bool AtcRackModel::spindle_has_probe_tool() const {
  return spindle_tool_.has_tool && (physical_tooling::is_firmware_probe_tool(spindle_tool_.tool) ||
                                    physical_tooling::is_probe_kind(spindle_tool_.kind));
}

bool AtcRackModel::detector_contact(Point3 spindle_position) const {
  if (!available_) {
    return false;
  }
  return std::any_of(pockets_.begin(), pockets_.end(), [spindle_position](const AtcPocketState& pocket) {
    return pocket.occupied &&
           physical_tooling::close_enough(spindle_position.x, pocket.position.x,
                                          physical_tooling::detector_tolerance_mm) &&
           physical_tooling::close_enough(spindle_position.y, pocket.position.y,
                                          physical_tooling::detector_tolerance_mm);
  });
}

void AtcRackModel::reconcile_spindle_tool_from_firmware(int active_tool) {
  if (spindle_tool_.has_tool || active_tool < 0) {
    return;
  }

  for (auto& pocket : pockets_) {
    if (!pocket.occupied || pocket.tool != active_tool) {
      continue;
    }
    spindle_tool_ = AtcSpindleState{pocket.tool, pocket.length_mm, pocket.kind, pocket.probe_tip_diameter_mm, true};
    pocket.occupied = false;
    return;
  }
}

}  // namespace sim
