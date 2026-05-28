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

#ifndef SIMULATOR_SIM_NONBLOCKING_FD_PUMP_HPP
#define SIMULATOR_SIM_NONBLOCKING_FD_PUMP_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

#include "sim/platform_io.hpp"

namespace sim {

struct NonblockingFdPumpOptions {
  bool close_on_read_error{true};
  bool close_on_write_error{true};
};

class NonblockingFdPump {
 public:
  NonblockingFdPump() = default;
  explicit NonblockingFdPump(platform_io::IoHandle fd);
  ~NonblockingFdPump();

  NonblockingFdPump(const NonblockingFdPump&) = delete;
  NonblockingFdPump& operator=(const NonblockingFdPump&) = delete;
  NonblockingFdPump(NonblockingFdPump&& other) noexcept;
  NonblockingFdPump& operator=(NonblockingFdPump&& other) noexcept;

  platform_io::IoHandle fd() const { return fd_; }
  bool open() const { return fd_ != platform_io::kInvalidHandle; }

  void reset(platform_io::IoHandle fd = platform_io::kInvalidHandle);
  void close();
  platform_io::IoHandle release();
  void clear_buffers();

  void queue_output(std::string_view bytes);
  std::string take_input();
  bool service(const NonblockingFdPumpOptions& options = {});

 private:
  platform_io::IoHandle fd_{platform_io::kInvalidHandle};
  std::string input_;
  std::string output_;
};

class BackgroundPoller {
 public:
  explicit BackgroundPoller(std::chrono::milliseconds interval = std::chrono::milliseconds(1));
  ~BackgroundPoller();

  BackgroundPoller(const BackgroundPoller&) = delete;
  BackgroundPoller& operator=(const BackgroundPoller&) = delete;

  void start(std::function<void()> callback);
  void stop();
  bool running() const { return running_.load(); }

 private:
  std::chrono::milliseconds interval_;
  std::atomic_bool running_{false};
  std::thread worker_;
};

}  // namespace sim

#endif
