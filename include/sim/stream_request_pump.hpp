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

#ifndef SIMULATOR_SIM_STREAM_REQUEST_PUMP_HPP
#define SIMULATOR_SIM_STREAM_REQUEST_PUMP_HPP

#include <functional>
#include <iosfwd>
#include <memory>

#include "carvera_sim.pb.h"

namespace sim {

class StreamRequestPump {
 public:
  using Handler = std::function<carvera::sim::v1::Response(const carvera::sim::v1::Request&)>;

  explicit StreamRequestPump(std::ostream& output);
  ~StreamRequestPump();

  StreamRequestPump(const StreamRequestPump&) = delete;
  StreamRequestPump& operator=(const StreamRequestPump&) = delete;

  bool closed() const;
  bool drain_available(const Handler& handler);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sim

#endif
