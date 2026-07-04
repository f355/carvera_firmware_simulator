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

#include "sim/interactive_io.hpp"

#include "sim/runtime_io.hpp"

namespace sim {

VirtualComPort::VirtualComPort(RuntimeIo& io) : runtime_io_(io) {}

VirtualComPort::~VirtualComPort() = default;

bool VirtualComPort::supported() const { return false; }

bool VirtualComPort::start() { return false; }

void VirtualComPort::stop() {}

void VirtualComPort::poll() {}

std::string VirtualComPort::poll_input() { return {}; }

void VirtualComPort::write_output(const std::string&) {}

void VirtualComPort::service_io_locked() {}

}  // namespace sim
