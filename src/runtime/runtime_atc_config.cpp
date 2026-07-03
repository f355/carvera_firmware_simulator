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

#include "sim/runtime_atc_config.hpp"

#include "Config.h"
#include "ConfigValue.h"
#include "Robot.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "sim/machine_geometry.hpp"

#include <cstdint>
#include <optional>

namespace sim::runtime_atc {
namespace {

constexpr double tool_setter_half_width_mm = 6.0;
constexpr double c1_tool_setter_button_height_mm = 8.0;
constexpr double c1_tool_setter_trigger_below_top_mm = 1.0;

constexpr uint16_t coordinate_checksum = CHECKSUM("coordinate");
constexpr uint16_t anchor1_x_checksum = CHECKSUM("anchor1_x");
constexpr uint16_t anchor1_y_checksum = CHECKSUM("anchor1_y");
constexpr uint16_t toolrack_offset_x_checksum = CHECKSUM("toolrack_offset_x");
constexpr uint16_t toolrack_offset_y_checksum = CHECKSUM("toolrack_offset_y");
constexpr uint16_t toolrack_z_checksum = CHECKSUM("toolrack_z");
constexpr uint16_t probe_mcs_x_checksum = CHECKSUM("probe_mcs_x");
constexpr uint16_t probe_mcs_y_checksum = CHECKSUM("probe_mcs_y");
constexpr uint16_t atc_checksum = CHECKSUM("atc");
constexpr uint16_t action_mm_checksum = CHECKSUM("action_mm");

MachineModel model_from_factory(char model) {
  return model == CARVERA_AIR ? MachineModel::CarveraAirCA1 : MachineModel::CarveraC1;
}

std::optional<int> active_tool_from_eeprom(Kernel& kernel, bool reconcile_persisted_tool) {
  if (!reconcile_persisted_tool || kernel.eeprom_data == nullptr) {
    return std::nullopt;
  }
  return kernel.eeprom_data->TOOL;
}

}  // namespace

PhysicalAtcConfig read_config(Kernel& kernel, bool reconcile_persisted_tool) {
  PhysicalAtcConfig config;
  if (kernel.factory_set != nullptr) {
    config.model = model_from_factory(kernel.factory_set->MachineModel);
  }
  config.persisted_active_tool = active_tool_from_eeprom(kernel, reconcile_persisted_tool);

  if (kernel.config == nullptr || kernel.robot == nullptr || kernel.factory_set == nullptr) {
    return config;
  }

  if (config.model == MachineModel::CarveraAirCA1) {
    if (const auto geometry = geometry_for(config.model)) {
      config.tool_setter_box = geometry->tool_setter;
    }
    return config;
  }

  if (kernel.factory_set->MachineModel != CARVERA) {
    return config;
  }

  config.atc_available = true;
  config.anchor1_x = kernel.config->value(coordinate_checksum, anchor1_x_checksum)->as_number(-359.0F);
  config.anchor1_y = kernel.config->value(coordinate_checksum, anchor1_y_checksum)->as_number(-234.0F);
  config.toolrack_offset_x = kernel.config->value(coordinate_checksum, toolrack_offset_x_checksum)->as_number(356.0F);
  config.toolrack_offset_y = kernel.config->value(coordinate_checksum, toolrack_offset_y_checksum)->as_number(0.0F);
  config.toolrack_z = kernel.config->value(coordinate_checksum, toolrack_z_checksum)->as_number(-105.0F);
  config.clamp_action_mm = kernel.config->value(atc_checksum, action_mm_checksum)->as_number(1.0F);

  const double default_probe_x = config.anchor1_x + config.toolrack_offset_x;
  const double default_probe_y = config.anchor1_y + config.toolrack_offset_y + 180.0;
  const double probe_x = kernel.config->value(coordinate_checksum, probe_mcs_x_checksum)->as_number(default_probe_x);
  const double probe_y = kernel.config->value(coordinate_checksum, probe_mcs_y_checksum)->as_number(default_probe_y);
  const double trigger_z = config.toolrack_z + c1_tool_setter_button_height_mm - c1_tool_setter_trigger_below_top_mm;
  config.tool_setter_box = Box{
      probe_x - tool_setter_half_width_mm, probe_y - tool_setter_half_width_mm, trigger_z - 2.0,
      probe_x + tool_setter_half_width_mm, probe_y + tool_setter_half_width_mm, trigger_z,
  };
  return config;
}

void configure_physical_scene(Kernel& kernel, PhysicalScene& scene, bool reconcile_persisted_tool) {
  scene.configure_atc(read_config(kernel, reconcile_persisted_tool));
}

}  // namespace sim::runtime_atc
