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

#include "sim/platform_io.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace sim::platform_io {
namespace {

constexpr std::size_t kMaxDrainBytesPerService = 16 * 1024;

SOCKET native_socket(IoHandle fd) { return static_cast<SOCKET>(fd); }

IoHandle io_handle(SOCKET socket) {
  return socket == INVALID_SOCKET ? kInvalidHandle : static_cast<IoHandle>(socket);
}

bool ensure_socket_runtime() {
  static std::once_flag once;
  static bool available = false;
  std::call_once(once, [] {
    WSADATA data{};
    available = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
  });
  return available;
}

int send_size(std::size_t size) {
  return static_cast<int>(std::min(size, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

}  // namespace

void set_nonblocking(IoHandle fd) {
  u_long enabled = 1;
  ::ioctlsocket(native_socket(fd), FIONBIO, &enabled);
}

void close_fd(IoHandle& fd) {
  if (fd != kInvalidHandle) {
    ::closesocket(native_socket(fd));
    fd = kInvalidHandle;
  }
}

std::string read_available(IoHandle fd, bool* still_open) {
  if (still_open != nullptr) {
    *still_open = true;
  }
  std::string bytes;
  char buffer[1024];
  for (;;) {
    const int count = ::recv(native_socket(fd), buffer, sizeof(buffer), 0);
    if (count > 0) {
      bytes.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count == SOCKET_ERROR) {
      const int error = ::WSAGetLastError();
      if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
        return bytes;
      }
    }
    if (still_open != nullptr) {
      *still_open = false;
    }
    return bytes;
  }
}

bool write_all(IoHandle fd, const std::string& bytes) {
  const char* ptr = bytes.data();
  auto left = bytes.size();
  while (left > 0) {
    const int count = ::send(native_socket(fd), ptr, send_size(left), 0);
    if (count > 0) {
      ptr += count;
      left -= static_cast<std::size_t>(count);
      continue;
    }
    if (count == SOCKET_ERROR) {
      const int error = ::WSAGetLastError();
      if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
        return false;
      }
    }
    return false;
  }
  return true;
}

bool drain_write_buffer(IoHandle fd, std::string& bytes) {
  std::size_t drained = 0;
  while (!bytes.empty() && drained < kMaxDrainBytesPerService) {
    const auto chunk = std::min(bytes.size(), kMaxDrainBytesPerService - drained);
    const int count = ::send(native_socket(fd), bytes.data(), send_size(chunk), 0);
    if (count > 0) {
      bytes.erase(0, static_cast<std::size_t>(count));
      drained += static_cast<std::size_t>(count);
      continue;
    }
    if (count == SOCKET_ERROR) {
      const int error = ::WSAGetLastError();
      if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
        return true;
      }
    }
    return false;
  }
  return true;
}

void set_raw_terminal(const std::string&) {}

IoHandle open_tcp_listener(std::uint16_t requested_port, std::uint16_t& bound_port) {
  if (!ensure_socket_runtime()) {
    return kInvalidHandle;
  }
  const SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
    return kInvalidHandle;
  }

  const BOOL yes = TRUE;
  ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(requested_port);
  if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
      ::listen(listener, 4) == SOCKET_ERROR) {
    ::closesocket(listener);
    return kInvalidHandle;
  }

  sockaddr_in bound{};
  int bound_len = sizeof(bound);
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &bound_len) == SOCKET_ERROR) {
    ::closesocket(listener);
    return kInvalidHandle;
  }
  bound_port = ntohs(bound.sin_port);

  const auto handle = io_handle(listener);
  set_nonblocking(handle);
  return handle;
}

IoHandle accept_pending_client(IoHandle listener) {
  if (!ensure_socket_runtime()) {
    return kInvalidHandle;
  }
  const SOCKET client = ::accept(native_socket(listener), nullptr, nullptr);
  if (client == INVALID_SOCKET) {
    return kInvalidHandle;
  }
  const auto handle = io_handle(client);
  set_nonblocking(handle);
  return handle;
}

IoHandle open_udp_socket() {
  if (!ensure_socket_runtime()) {
    return kInvalidHandle;
  }
  return io_handle(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
}

bool send_udp_loopback(IoHandle socket, std::uint16_t port, std::string_view payload) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  return ::sendto(native_socket(socket), payload.data(), send_size(payload.size()), 0,
                  reinterpret_cast<sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR;
}

}  // namespace sim::platform_io
