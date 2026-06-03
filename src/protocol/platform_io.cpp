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
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace sim::platform_io {
namespace {

constexpr std::size_t kMaxDrainBytesPerService = 16 * 1024;

}  // namespace

bool ensure_socket_runtime() {
#ifdef _WIN32
  static const bool initialized = []() {
    WSADATA data{};
    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return initialized;
#else
  return true;
#endif
}

void set_nonblocking(IoHandle fd) {
#ifndef _WIN32
  const int flags = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(static_cast<int>(fd), F_SETFL, flags | O_NONBLOCK);
  }
#else
  if (fd != kInvalidHandle) {
    u_long mode = 1;
    ::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode);
  }
#endif
}

void close_fd(IoHandle& fd) {
#ifndef _WIN32
  if (fd != kInvalidHandle) {
    ::close(static_cast<int>(fd));
    fd = kInvalidHandle;
  }
#else
  if (fd != kInvalidHandle) {
    ::closesocket(static_cast<SOCKET>(fd));
    fd = kInvalidHandle;
  }
#endif
}

std::string read_available(IoHandle fd, bool* still_open) {
  if (still_open != nullptr) {
    *still_open = true;
  }
#ifdef _WIN32
  std::string bytes;
  char buffer[1024];
  for (;;) {
    const auto n = ::recv(static_cast<SOCKET>(fd), buffer, sizeof(buffer), 0);
    if (n > 0) {
      bytes.append(buffer, static_cast<std::size_t>(n));
      continue;
    }
    if (n < 0) {
      const auto error = ::WSAGetLastError();
      if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
        return bytes;
      }
    }
    if (still_open != nullptr) {
      *still_open = false;
    }
    return bytes;
  }
#else
  std::string bytes;
  char buffer[1024];
  for (;;) {
    const auto n = ::read(static_cast<int>(fd), buffer, sizeof(buffer));
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
#endif
}

bool write_all(IoHandle fd, const std::string& bytes) {
#ifdef _WIN32
  auto ptr = bytes.data();
  auto left = bytes.size();
  while (left > 0) {
    const auto n = ::send(static_cast<SOCKET>(fd), ptr, static_cast<int>(left), 0);
    if (n > 0) {
      ptr += n;
      left -= static_cast<std::size_t>(n);
      continue;
    }
    const auto error = ::WSAGetLastError();
    if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
      return false;
    }
    return false;
  }
  return true;
#else
  const char* ptr = bytes.data();
  auto left = bytes.size();
  while (left > 0) {
    const auto n = ::write(static_cast<int>(fd), ptr, left);
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
#endif
}

bool drain_write_buffer(IoHandle fd, std::string& bytes) {
#ifdef _WIN32
  std::size_t drained = 0;
  while (!bytes.empty() && drained < kMaxDrainBytesPerService) {
    const auto chunk = std::min(bytes.size(), kMaxDrainBytesPerService - drained);
    const auto n = ::send(static_cast<SOCKET>(fd), bytes.data(), static_cast<int>(chunk), 0);
    if (n > 0) {
      bytes.erase(0, static_cast<std::size_t>(n));
      drained += static_cast<std::size_t>(n);
      continue;
    }
    const auto error = ::WSAGetLastError();
    if (error == WSAEWOULDBLOCK || error == WSAEINTR) {
      return true;
    }
    return false;
  }
  return true;
#else
  std::size_t drained = 0;
  while (!bytes.empty() && drained < kMaxDrainBytesPerService) {
    const auto chunk = std::min(bytes.size(), kMaxDrainBytesPerService - drained);
    const auto n = ::write(static_cast<int>(fd), bytes.data(), chunk);
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
#endif
}

void set_raw_terminal(const std::string& path) {
#ifndef _WIN32
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
#else
  (void)path;
#endif
}

}  // namespace sim::platform_io
