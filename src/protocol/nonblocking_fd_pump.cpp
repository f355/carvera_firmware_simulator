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

#include "sim/nonblocking_fd_pump.hpp"

#include <utility>

#include "sim/platform_io.hpp"

namespace sim {

NonblockingFdPump::NonblockingFdPump(platform_io::IoHandle fd) : fd_(fd) {}

NonblockingFdPump::~NonblockingFdPump() { close(); }

NonblockingFdPump::NonblockingFdPump(NonblockingFdPump&& other) noexcept
    : fd_(other.release()), input_(std::move(other.input_)), output_(std::move(other.output_)) {}

NonblockingFdPump& NonblockingFdPump::operator=(NonblockingFdPump&& other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.release();
    input_ = std::move(other.input_);
    output_ = std::move(other.output_);
  }
  return *this;
}

void NonblockingFdPump::reset(platform_io::IoHandle fd) {
  close();
  fd_ = fd;
}

void NonblockingFdPump::close() {
  platform_io::close_fd(fd_);
  clear_buffers();
}

platform_io::IoHandle NonblockingFdPump::release() {
  const auto fd = fd_;
  fd_ = platform_io::kInvalidHandle;
  return fd;
}

void NonblockingFdPump::clear_buffers() {
  input_.clear();
  output_.clear();
}

void NonblockingFdPump::queue_output(std::string_view bytes) {
  if (!bytes.empty()) {
    output_.append(bytes);
  }
}

std::string NonblockingFdPump::take_input() {
  auto input = std::move(input_);
  input_.clear();
  return input;
}

bool NonblockingFdPump::service(const NonblockingFdPumpOptions& options) {
  if (fd_ == platform_io::kInvalidHandle) {
    return false;
  }

  bool still_open = true;
  input_.append(platform_io::read_available(fd_, &still_open));
  if (!still_open && options.close_on_read_error) {
    close();
    return false;
  }

  if (!output_.empty() && !platform_io::drain_write_buffer(fd_, output_) && options.close_on_write_error) {
    close();
    return false;
  }

  return fd_ != platform_io::kInvalidHandle;
}

BackgroundPoller::BackgroundPoller(std::chrono::milliseconds interval) : interval_(interval) {}

BackgroundPoller::~BackgroundPoller() { stop(); }

void BackgroundPoller::start(std::function<void()> callback) {
  stop();
  running_.store(true);
  worker_ = std::thread([this, callback = std::move(callback)]() {
    while (running_.load()) {
      callback();
      std::this_thread::sleep_for(interval_);
    }
  });
}

void BackgroundPoller::stop() {
  running_.store(false);
  if (worker_.joinable()) {
    worker_.join();
  }
}

}  // namespace sim
