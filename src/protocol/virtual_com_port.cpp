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

#include "sim/firmware_runtime.hpp"
#include "sim/platform_io.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <stdlib.h>
#endif

namespace sim {

VirtualComPort::VirtualComPort(FirmwareRuntime& runtime) : runtime_(runtime) {}

VirtualComPort::~VirtualComPort() { stop(); }

bool VirtualComPort::supported() const {
#ifdef _WIN32
  return false;
#else
  return true;
#endif
}

bool VirtualComPort::start() {
#ifdef _WIN32
  return false;
#else
  if (io_.open()) {
    return true;
  }

  platform_io::IoHandle master_fd = ::posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd == platform_io::kInvalidHandle) {
    return false;
  }
  if (::grantpt(static_cast<int>(master_fd)) != 0 || ::unlockpt(static_cast<int>(master_fd)) != 0) {
    platform_io::close_fd(master_fd);
    stop();
    return false;
  }

  char* slave = ::ptsname(static_cast<int>(master_fd));
  if (slave == nullptr) {
    platform_io::close_fd(master_fd);
    stop();
    return false;
  }
  device_path_ = slave;
  platform_io::set_raw_terminal(device_path_);
  platform_io::set_nonblocking(master_fd);
  io_.reset(master_fd);
  worker_.start([this]() {
    std::lock_guard<std::mutex> lock(mutex_);
    service_io_locked();
  });
  return true;
#endif
}

void VirtualComPort::stop() {
  worker_.stop();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    io_.close();
  }
  device_path_.clear();
}

void VirtualComPort::poll() {
#ifndef _WIN32
  poll_input();
  write_output(runtime_.read_serial());
#endif
}

std::string VirtualComPort::poll_input() {
#ifndef _WIN32
  if (!io_.open()) {
    return {};
  }

  std::string input;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    service_io_locked();
    input = io_.take_input();
  }
  if (!input.empty()) {
    runtime_.write_serial(input);
  }
  return input;
#else
  return {};
#endif
}

void VirtualComPort::write_output(const std::string& bytes) {
  if (!io_.open() || bytes.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  io_.queue_output(bytes);
  service_io_locked();
}

void VirtualComPort::service_io_locked() {
#ifndef _WIN32
  NonblockingFdPumpOptions options;
  options.close_on_read_error = false;
  options.close_on_write_error = false;
  io_.service(options);
#endif
}

}  // namespace sim
