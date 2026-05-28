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

#ifndef SIMULATOR_SIM_PLATFORM_IO_HPP
#define SIMULATOR_SIM_PLATFORM_IO_HPP

#include <cstdint>
#include <string>

namespace sim::platform_io {

using IoHandle = std::intptr_t;

constexpr IoHandle kInvalidHandle = -1;

bool ensure_socket_runtime();
void set_nonblocking(IoHandle fd);
void close_fd(IoHandle& fd);
std::string read_available(IoHandle fd, bool* still_open = nullptr);
bool write_all(IoHandle fd, const std::string& bytes);
bool drain_write_buffer(IoHandle fd, std::string& bytes);
void set_raw_terminal(const std::string& path);

}  // namespace sim::platform_io

#endif
