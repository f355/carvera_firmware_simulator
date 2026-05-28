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

#ifndef SIMULATOR_SIM_FRAMED_PROTO_HPP
#define SIMULATOR_SIM_FRAMED_PROTO_HPP

#include <google/protobuf/message_lite.h>

#include <iosfwd>

namespace sim::proto_framing {

bool write_message(std::ostream& stream, const google::protobuf::MessageLite& message);
bool read_message(std::istream& stream, google::protobuf::MessageLite& message);

}  // namespace sim::proto_framing

#endif
