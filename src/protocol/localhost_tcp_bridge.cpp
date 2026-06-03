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

#include "sim/firmware_runtime.hpp"
#include "sim/m8266_wifi.hpp"
#include "sim/platform_io.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace sim {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
#else
using NativeSocket = int;
#endif

NativeSocket native_socket(platform_io::IoHandle handle) { return static_cast<NativeSocket>(handle); }

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

LocalhostTcpBridge::LocalhostTcpBridge(FirmwareRuntime& runtime) : runtime_(runtime) {}

LocalhostTcpBridge::~LocalhostTcpBridge() { stop(); }

bool LocalhostTcpBridge::start(std::uint16_t requested_port) {
  if (listen_fd_ != platform_io::kInvalidHandle) {
    return true;
  }
  if (!platform_io::ensure_socket_runtime()) {
    return false;
  }

  const auto listen_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
  if (listen_socket == INVALID_SOCKET) {
    return false;
  }
#else
  if (listen_socket < 0) {
    return false;
  }
#endif
  listen_fd_ = static_cast<platform_io::IoHandle>(listen_socket);

  int yes = 1;
  ::setsockopt(native_socket(listen_fd_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(requested_port);
  if (::bind(native_socket(listen_fd_), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    stop();
    return false;
  }
  if (::listen(native_socket(listen_fd_), 4) != 0) {
    stop();
    return false;
  }

  sockaddr_in bound{};
#ifdef _WIN32
  int bound_len = sizeof(bound);
#else
  socklen_t bound_len = sizeof(bound);
#endif
  if (::getsockname(native_socket(listen_fd_), reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
    port_ = ntohs(bound.sin_port);
  }
  platform_io::set_nonblocking(listen_fd_);
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
  write_output(runtime_.read_wifi_tcp());
}

std::string LocalhostTcpBridge::poll_input() {
  if (listen_fd_ == platform_io::kInvalidHandle) {
    return {};
  }

  std::string combined;
  std::string firmware_input;
  bool connected = false;
  const bool firmware_uploading = runtime_.is_uploading();
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
    runtime_.write_wifi_tcp(firmware_input);
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
  if (connected) {
    m8266_wifi::active().connect_tcp_client();
  } else {
    m8266_wifi::active().disconnect_tcp_client();
  }
}

void LocalhostTcpBridge::accept_pending_clients() {
  for (;;) {
    const auto client = ::accept(native_socket(listen_fd_), nullptr, nullptr);
#ifdef _WIN32
    if (client == INVALID_SOCKET) {
      break;
    }
#else
    if (client < 0) {
      break;
    }
#endif
#ifdef SO_NOSIGPIPE
    int yes = 1;
    ::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
    platform_io::set_nonblocking(client);

    Client state;
    state.io.reset(static_cast<platform_io::IoHandle>(client));
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
