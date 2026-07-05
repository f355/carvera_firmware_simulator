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
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

namespace sim::platform_io {
namespace {

constexpr std::size_t kMaxDrainBytesPerService = 16 * 1024;

int native_fd(IoHandle fd) { return static_cast<int>(fd); }

IoHandle io_handle(int fd) { return fd < 0 ? kInvalidHandle : static_cast<IoHandle>(fd); }

}  // namespace

void set_nonblocking(IoHandle fd) {
  const int flags = ::fcntl(native_fd(fd), F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(native_fd(fd), F_SETFL, flags | O_NONBLOCK);
  }
}

void close_fd(IoHandle& fd) {
  if (fd != kInvalidHandle) {
    ::close(native_fd(fd));
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
    const auto n = ::read(native_fd(fd), buffer, sizeof(buffer));
    if (n > 0) {
      bytes.append(buffer, static_cast<std::size_t>(n));
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      return bytes;
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
    const auto n = ::write(native_fd(fd), ptr, left);
    if (n > 0) {
      ptr += n;
      left -= static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      return false;
    }
    return false;
  }
  return true;
}

bool drain_write_buffer(IoHandle fd, std::string& bytes) {
  std::size_t drained = 0;
  while (!bytes.empty() && drained < kMaxDrainBytesPerService) {
    const auto chunk = std::min(bytes.size(), kMaxDrainBytesPerService - drained);
    const auto n = ::write(native_fd(fd), bytes.data(), chunk);
    if (n > 0) {
      bytes.erase(0, static_cast<std::size_t>(n));
      drained += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
      return true;
    }
    return false;
  }
  return true;
}

void set_raw_terminal(const std::string& path) {
  const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY);
  if (fd < 0) {
    return;
  }
  termios tio{};
  if (::tcgetattr(fd, &tio) == 0) {
    ::cfmakeraw(&tio);
    ::tcsetattr(fd, TCSANOW, &tio);
  }
  ::close(fd);
}

IoHandle open_tcp_listener(std::uint16_t requested_port, std::uint16_t& bound_port) {
  const int listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener < 0) {
    return kInvalidHandle;
  }

  int yes = 1;
  ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(requested_port);
  if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(listener, 4) != 0) {
    ::close(listener);
    return kInvalidHandle;
  }

  sockaddr_in bound{};
  socklen_t bound_len = sizeof(bound);
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0) {
    ::close(listener);
    return kInvalidHandle;
  }
  bound_port = ntohs(bound.sin_port);

  const auto handle = io_handle(listener);
  set_nonblocking(handle);
  return handle;
}

IoHandle accept_pending_client(IoHandle listener) {
  const int client = ::accept(native_fd(listener), nullptr, nullptr);
  if (client < 0) {
    return kInvalidHandle;
  }
#ifdef SO_NOSIGPIPE
  int yes = 1;
  ::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
  const auto handle = io_handle(client);
  set_nonblocking(handle);
  return handle;
}

IoHandle open_udp_socket() { return io_handle(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)); }

bool send_udp_loopback(IoHandle socket, std::uint16_t port, std::string_view payload) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  return ::sendto(native_fd(socket), payload.data(), payload.size(), 0, reinterpret_cast<sockaddr*>(&address),
                  sizeof(address)) >= 0;
}

}  // namespace sim::platform_io
