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

#ifndef SIMULATOR_SIM_INTERACTIVE_IO_HPP
#define SIMULATOR_SIM_INTERACTIVE_IO_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "sim/nonblocking_fd_pump.hpp"
#include "sim/platform_io.hpp"

namespace sim {

class RuntimeIo;

class VirtualComPort {
 public:
  explicit VirtualComPort(RuntimeIo& io);
  ~VirtualComPort();

  bool start();
  void stop();
  void poll();
  std::string poll_input();
  void write_output(const std::string& bytes);

  const std::string& device_path() const { return device_path_; }
  bool supported() const;

 private:
  void service_io_locked();

  RuntimeIo& runtime_io_;
  std::string device_path_;
  platform_io::IoHandle slave_anchor_fd_{platform_io::kInvalidHandle};
  NonblockingFdPump io_;
  BackgroundPoller worker_;
  std::mutex mutex_;
};

class LocalhostTcpBridge {
 public:
  using UploadingQuery = std::function<bool()>;

  explicit LocalhostTcpBridge(RuntimeIo& io, UploadingQuery uploading = [] { return false; });
  ~LocalhostTcpBridge();

  bool start(std::uint16_t requested_port);
  void stop();
  void poll();
  std::string poll_input();
  void write_output(const std::string& bytes);
  std::size_t queued_output_bytes() const;

  std::uint16_t port() const { return port_; }

 private:
  struct Client {
    NonblockingFdPump io;
    std::string pending_input;
  };

  void accept_pending_clients();
  void service_clients_locked();
  bool service_client_io_locked(Client& client);
  void update_firmware_connection_state(bool connected);
  std::string take_pending_firmware_input(Client& client, bool firmware_uploading) const;

  RuntimeIo& runtime_io_;
  UploadingQuery uploading_;
  platform_io::IoHandle listen_fd_{platform_io::kInvalidHandle};
  std::vector<Client> clients_;
  std::uint16_t port_{0};
  BackgroundPoller worker_;
  mutable std::mutex mutex_;
};

class LocalhostDiscoveryBeacon {
 public:
  LocalhostDiscoveryBeacon();
  ~LocalhostDiscoveryBeacon();

  bool start();
  void stop();
  void publish(std::string_view payload);
  void poll();

 private:
  void send_payload(std::string_view payload) const;

  std::string payload_;
  std::chrono::steady_clock::time_point next_send_{};
  platform_io::IoHandle socket_fd_{platform_io::kInvalidHandle};
};

}  // namespace sim

#endif
