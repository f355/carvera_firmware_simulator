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

#ifndef SIMULATOR_SIM_LOGGING_HPP
#define SIMULATOR_SIM_LOGGING_HPP

#include <string>
#include <string_view>

namespace sim::logging {

std::string elapsed_timestamp();
std::string printable_bytes(std::string_view bytes);
void event(std::string_view category, std::string_view message);
void traffic(std::string_view channel, std::string_view direction, std::string_view bytes);

}  // namespace sim::logging

#endif
