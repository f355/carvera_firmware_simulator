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

#include "sim/physical_signal_driver.hpp"

#include "sim/board_profile.hpp"
#include "sim/gpio_level.hpp"

namespace sim {
namespace {

void drive_signal(const BoardSignal& signal, bool active) {
  if (signal.connected) {
    gpio::set_level(signal.pin, signal_level(signal, active));
  }
}

}  // namespace

void PhysicalSignalDriver::drive(MachineModel model, const ProbeContactState& probe_contacts,
                                 bool atc_detector_contact) {
  const auto& profile = board_profile(model);
  drive_signal(profile.physical_probe, probe_contacts.probe_contact);
  drive_signal(profile.physical_tool_setter, probe_contacts.tool_setter_contact);
  drive_signal(profile.physical_atc_detector, atc_detector_contact);
}

}  // namespace sim
