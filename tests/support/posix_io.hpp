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

#ifndef SIMULATOR_TESTS_SUPPORT_POSIX_IO_HPP
#define SIMULATOR_TESTS_SUPPORT_POSIX_IO_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sim::test {

inline bool write_exact(int fd, const void* data, std::size_t size) {
#ifdef _WIN32
  (void)fd;
  (void)data;
  (void)size;
  return false;
#else
  auto* bytes = static_cast<const char*>(data);
  while (size > 0) {
    const auto written = ::write(fd, bytes, size);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    bytes += written;
    size -= static_cast<std::size_t>(written);
  }
  return true;
#endif
}

inline bool read_exact(int fd, void* data, std::size_t size) {
#ifdef _WIN32
  (void)fd;
  (void)data;
  (void)size;
  return false;
#else
  auto* bytes = static_cast<char*>(data);
  while (size > 0) {
    const auto received = ::read(fd, bytes, size);
    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (received == 0) {
      return false;
    }
    bytes += received;
    size -= static_cast<std::size_t>(received);
  }
  return true;
#endif
}

inline bool read_exact_timeout(int fd, void* data, std::size_t size, std::chrono::milliseconds timeout) {
#ifdef _WIN32
  (void)fd;
  (void)data;
  (void)size;
  (void)timeout;
  return false;
#else
  auto* bytes = static_cast<char*>(data);
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (size > 0 && std::chrono::steady_clock::now() < deadline) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeval tv{0, 100000};
    const int ready = ::select(fd + 1, &read_set, nullptr, nullptr, &tv);
    if (ready <= 0 || !FD_ISSET(fd, &read_set)) {
      continue;
    }
    const auto received = ::read(fd, bytes, size);
    if (received <= 0) {
      return false;
    }
    bytes += received;
    size -= static_cast<std::size_t>(received);
  }
  return size == 0;
#endif
}

inline std::string read_available(int fd, std::size_t buffer_size = 1024) {
  std::string output;
#ifndef _WIN32
  std::string buffer(buffer_size, '\0');
  for (;;) {
    const auto received = ::read(fd, buffer.data(), buffer.size());
    if (received > 0) {
      output.append(buffer.data(), static_cast<std::size_t>(received));
      continue;
    }
    if (received < 0 && errno == EINTR) {
      continue;
    }
    return output;
  }
#else
  (void)fd;
  (void)buffer_size;
  return output;
#endif
}

inline std::string read_until(int fd, const std::string& needle,
                              std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  std::string output;
#ifndef _WIN32
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeval tv{0, 100000};
    const int ready = ::select(fd + 1, &read_set, nullptr, nullptr, &tv);
    if (ready > 0 && FD_ISSET(fd, &read_set)) {
      char buffer[512];
      const auto received = ::read(fd, buffer, sizeof(buffer));
      if (received > 0) {
        output.append(buffer, static_cast<std::size_t>(received));
        if (output.find(needle) != std::string::npos) {
          return output;
        }
      }
    }
  }
#else
  (void)fd;
  (void)needle;
  (void)timeout;
#endif
  return output;
}

inline bool connect_loopback(std::uint16_t port, int& client, int attempts = 50) {
#ifdef _WIN32
  (void)port;
  (void)client;
  (void)attempts;
  return false;
#else
  client = ::socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  for (int i = 0; i < attempts; ++i) {
    if (::connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ::close(client);
  client = -1;
  return false;
#endif
}

inline bool localhost_accepts_tcp(std::uint32_t port) {
  int client = -1;
  if (!connect_loopback(static_cast<std::uint16_t>(port), client, 1)) {
    return false;
  }
#ifndef _WIN32
  ::close(client);
#endif
  return true;
}

inline int open_virtual_com_slave(const std::string& path) {
#ifdef _WIN32
  (void)path;
  return -1;
#else
  return ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
#endif
}

inline bool set_nonblocking(int fd) {
#ifdef _WIN32
  (void)fd;
  return false;
#else
  const int flags = ::fcntl(fd, F_GETFL, 0);
  return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

}  // namespace sim::test

#endif
