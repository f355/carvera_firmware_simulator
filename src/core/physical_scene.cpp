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

#include "sim/physical_scene.hpp"

#include "sim/machine_geometry.hpp"

namespace sim {

void PhysicalScene::clear() {
  model_ = MachineModel::CarveraC1;
  probe_contacts_.clear();
  atc_rack_.clear();
  last_contacts_ = {};
  update_probe_contacts({});
}

bool PhysicalScene::probe_tool_installed() const {
  return probe_contacts_.probe_tool_installed() || atc_rack_.spindle_has_probe_tool();
}

void PhysicalScene::set_atc_pocket_tool(int pocket, int tool, bool occupied, double length_mm, ToolKind kind,
                                        double probe_tip_diameter_mm) {
  atc_rack_.set_pocket_tool(pocket, tool, occupied, length_mm, kind, probe_tip_diameter_mm);
}

void PhysicalScene::clear_atc_pocket_tools() {
  atc_rack_.clear_pocket_tools();
  sync_probe_install_from_spindle();
}

void PhysicalScene::configure_atc(const PhysicalAtcConfig& config) {
  model_ = config.model;
  probe_contacts_.configure_tool_setter(config.tool_setter_box);
  const auto geometry = geometry_for(model_);
  if (geometry.has_value()) {
    const Box& travel = geometry->physical_travel;
    probe_contacts_.configure_bed(Box{
        travel.min_x,
        travel.min_y,
        geometry->bed_z,
        travel.max_x,
        travel.max_y,
        geometry->bed_z,
    });
  } else {
    probe_contacts_.configure_bed(std::nullopt);
  }
  atc_rack_.configure(config);
}

void PhysicalScene::update_atc_clamp_position(Point3 spindle_position, double clamp_position_mm) {
  if (atc_rack_.update_clamp_position(spindle_position, clamp_position_mm)) {
    sync_probe_install_from_spindle();
  }
}

void PhysicalScene::atc_clamp_open(Point3 spindle_position) {
  atc_rack_.clamp_open(spindle_position);
  sync_probe_install_from_spindle();
}

void PhysicalScene::atc_clamp_close(Point3 spindle_position) {
  atc_rack_.clamp_close(spindle_position);
  sync_probe_install_from_spindle();
}

void PhysicalScene::set_spindle_tool(int tool, double length_mm, bool installed, ToolKind kind,
                                     double probe_tip_diameter_mm) {
  atc_rack_.set_spindle_tool(tool, length_mm, installed, kind, probe_tip_diameter_mm);
  sync_probe_install_from_spindle();
}

void PhysicalScene::update_probe_contacts(Point3 spindle_probe_position) {
  last_contacts_ = probe_contacts_.update(spindle_probe_position, atc_rack_.spindle());
  signal_driver_.drive(model_, last_contacts_, atc_rack_.detector_contact(spindle_probe_position));
}

void PhysicalScene::sync_probe_install_from_spindle() {
  probe_contacts_.set_probe_tool_installed(atc_rack_.spindle_has_probe_tool());
}

}  // namespace sim
