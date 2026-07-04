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

#include <utility>

#include "sim/platform_io.hpp"
#include "sim/runtime_io.hpp"

namespace sim {

namespace {

bool is_realtime_command_byte(char byte) {
  switch (byte) {
    case '?':
    case '!':
    case '~':
    case '\x18':  // Ctrl-X: abort
    case '\x19':  // Ctrl-Y: stop
    case '\x1a':  // Ctrl-Z: keepalive
      return true;
    default:
      return false;
  }
}

}  // namespace

std::string LocalhostTcpBridge::take_pending_firmware_input(Client& client, bool firmware_uploading) const {
  if (client.pending_input.empty()) {
    return {};
  }

  if (firmware_uploading) {
    auto bytes = std::move(client.pending_input);
    client.pending_input.clear();
    return bytes;
  }

  if (is_realtime_command_byte(client.pending_input.front())) {
    const auto count =
        client.pending_input.size() >= 2 && client.pending_input[0] == '?' && client.pending_input[1] == '1' ? 2U : 1U;
    auto bytes = client.pending_input.substr(0, count);
    client.pending_input.erase(0, count);
    return bytes;
  }

  const auto newline = client.pending_input.find('\n');
  if (newline == std::string::npos) {
    return {};
  }

  auto bytes = client.pending_input.substr(0, newline + 1);
  client.pending_input.erase(0, newline + 1);
  return bytes;
}

LocalhostTcpBridge::LocalhostTcpBridge(RuntimeIo& io, UploadingQuery uploading)
    : runtime_io_(io), uploading_(std::move(uploading)) {}

LocalhostTcpBridge::~LocalhostTcpBridge() { stop(); }

bool LocalhostTcpBridge::start(std::uint16_t requested_port) {
  if (listen_fd_ != platform_io::kInvalidHandle) {
    return true;
  }
  listen_fd_ = platform_io::open_loopback_tcp_listener(requested_port, port_);
  if (listen_fd_ == platform_io::kInvalidHandle) {
    return false;
  }
  worker_.start([this]() {
    accept_pending_clients();
    std::lock_guard<std::mutex> lock(mutex_);
    service_clients_locked();
  });
  return true;
}

void LocalhostTcpBridge::stop() {
  worker_.stop();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& client : clients_) {
      client.io.close();
    }
    clients_.clear();
  }
  update_firmware_connection_state(false);
  platform_io::close_fd(listen_fd_);
  port_ = 0;
}

void LocalhostTcpBridge::poll() {
  poll_input();
  write_output(runtime_io_.read_wifi_tcp());
}

std::string LocalhostTcpBridge::poll_input() {
  if (listen_fd_ == platform_io::kInvalidHandle) {
    return {};
  }

  std::string combined;
  std::string firmware_input;
  bool connected = false;
  const bool firmware_uploading = uploading_();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    service_clients_locked();
    connected = !clients_.empty();
    for (auto& client : clients_) {
      auto input = client.io.take_input();
      if (!input.empty()) {
        combined.append(input);
        client.pending_input.append(std::move(input));
      }
      firmware_input.append(take_pending_firmware_input(client, firmware_uploading));
    }
  }
  update_firmware_connection_state(connected);
  if (!firmware_input.empty()) {
    runtime_io_.write_wifi_tcp(firmware_input);
  }
  return combined;
}

void LocalhostTcpBridge::write_output(const std::string& bytes) {
  if (bytes.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = clients_.begin(); it != clients_.end();) {
    it->io.queue_output(bytes);
    if (!service_client_io_locked(*it)) {
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

std::size_t LocalhostTcpBridge::queued_output_bytes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::size_t total = 0;
  for (const auto& client : clients_) {
    total += client.io.queued_output_size();
  }
  return total;
}

bool LocalhostTcpBridge::service_client_io_locked(Client& client) { return client.io.service(); }

void LocalhostTcpBridge::update_firmware_connection_state(bool connected) {
  runtime_io_.set_wifi_client_connected(connected);
}

void LocalhostTcpBridge::accept_pending_clients() {
  for (;;) {
    const auto client = platform_io::accept_pending_client(listen_fd_);
    if (client == platform_io::kInvalidHandle) {
      break;
    }

    Client state;
    state.io.reset(client);
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(std::move(state));
  }
}

void LocalhostTcpBridge::service_clients_locked() {
  for (auto it = clients_.begin(); it != clients_.end();) {
    if (!service_client_io_locked(*it)) {
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace sim
