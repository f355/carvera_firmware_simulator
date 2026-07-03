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
#include <string>

#include "ATCHandlerPublicAccess.h"
#include "Gcode.h"
#include "Kernel.h"
#include "PublicData.h"
#include "StreamOutput.h"
#include "sim/simulation_instance.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  sim::SimulationInstance simulation;
  auto& runtime = simulation.firmware();
  runtime.boot();

  runtime.run_main_loop(1);
  require(runtime.read_wireless_probe_tx() == "Q", "SerialConsole2 should query the wireless probe over UART on boot");

  runtime.write_wireless_probe_rx("V3.55\n");
  runtime.run_main_loop(2);
  float voltage = 0.0F;
  require(PublicData::get_value(atc_handler_checksum, get_wp_voltage_checksum, &voltage),
          "SerialConsole2 should publish wireless probe voltage through PublicData");
  require(voltage > 3.54F && voltage < 3.56F, "wireless probe voltage should be parsed from the Zigbee protocol line");

  Gcode pair("M471", &StreamOutput::NullStream);
  Kernel::instance->call_event(ON_GCODE_RECEIVED, &pair);
  require(runtime.read_wireless_probe_tx().find('P') != std::string::npos,
          "M471 should send the wireless probe pairing command over the second UART");

  return 0;
}
