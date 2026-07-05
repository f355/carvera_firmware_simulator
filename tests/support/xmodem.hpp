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

#ifndef SIMULATOR_TESTS_SUPPORT_XMODEM_HPP
#define SIMULATOR_TESTS_SUPPORT_XMODEM_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include "posix_io.hpp"

namespace sim::test {

inline std::uint16_t crc16_xmodem(const std::string& data) {
  std::uint16_t crc = 0;
  for (const auto byte : data) {
    crc ^= static_cast<std::uint16_t>(static_cast<unsigned char>(byte)) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
  }
  return crc;
}

template <typename Pump>
std::string receive_xmodem_download(int fd, Pump&& pump) {
  constexpr char stx = '\x02';
  constexpr char eot = '\x04';
  constexpr char ack = '\x06';
  constexpr char nak = '\x15';
  constexpr char crc = 'C';
  constexpr std::size_t packet_size = 8192;

  std::string received_file;
  unsigned expected_sequence = 0;
  char next = 0;
  for (int attempts = 0; attempts < 80; ++attempts) {
    const char request = expected_sequence == 0 ? crc : nak;
    if (!write_exact(fd, &request, 1)) {
      return {};
    }
    if (read_exact_timeout_pumping(fd, &next, 1, std::chrono::milliseconds(500), pump)) {
      break;
    }
  }

  while (next != 0) {
    if (next == eot) {
      return write_exact(fd, &ack, 1) ? received_file : std::string{};
    }
    if (next != stx) {
      if (!read_exact_timeout_pumping(fd, &next, 1, std::chrono::seconds(2), pump)) {
        return {};
      }
      continue;
    }

    char header[2]{};
    std::string payload(2 + packet_size + 2, '\0');
    if (!read_exact_timeout_pumping(fd, header, sizeof(header), std::chrono::seconds(2), pump) ||
        !read_exact_timeout_pumping(fd, payload.data(), payload.size(), std::chrono::seconds(2), pump)) {
      return {};
    }

    const auto sequence = static_cast<unsigned>(static_cast<unsigned char>(header[0]));
    const auto complement = static_cast<unsigned>(static_cast<unsigned char>(header[1]));
    if (sequence != expected_sequence || complement != (0xffu - expected_sequence)) {
      return {};
    }

    const auto crc_offset = payload.size() - 2;
    const auto received_crc = static_cast<std::uint16_t>((static_cast<unsigned char>(payload[crc_offset]) << 8) |
                                                         static_cast<unsigned char>(payload[crc_offset + 1]));
    if (crc16_xmodem(payload.substr(0, crc_offset)) != received_crc) {
      return {};
    }

    const auto data_len = (static_cast<unsigned>(static_cast<unsigned char>(payload[0])) << 8) |
                          static_cast<unsigned>(static_cast<unsigned char>(payload[1]));
    if (data_len > packet_size) {
      return {};
    }
    if (expected_sequence > 0) {
      received_file.append(payload.data() + 2, data_len);
    }

    if (!write_exact(fd, &ack, 1)) {
      return {};
    }
    expected_sequence = (expected_sequence + 1) & 0xffu;
    if (!read_exact_timeout_pumping(fd, &next, 1, std::chrono::seconds(2), pump)) {
      return {};
    }
  }
  return {};
}

inline std::string receive_xmodem_download(int fd) {
  return receive_xmodem_download(fd, [] {});
}

template <typename Pump>
bool send_xmodem_upload(int fd, std::string_view contents, Pump&& pump) {
  constexpr char stx = '\x02';
  constexpr char eot = '\x04';
  constexpr char ack = '\x06';
  constexpr std::size_t packet_size = 8192;

  char response = 0;
  for (int attempts = 0; attempts < 80 && response != 'C'; ++attempts) {
    if (!read_exact_timeout_pumping(fd, &response, 1, std::chrono::milliseconds(500), pump)) {
      response = 0;
    }
  }
  if (response != 'C') {
    return false;
  }

  const auto send_packet = [&](unsigned sequence, std::string_view payload) {
    if (payload.size() > packet_size) {
      return false;
    }
    std::string crc_input;
    crc_input.reserve(2 + packet_size);
    crc_input.push_back(static_cast<char>((payload.size() >> 8) & 0xffu));
    crc_input.push_back(static_cast<char>(payload.size() & 0xffu));
    crc_input.append(payload);
    crc_input.resize(2 + packet_size, '\x1a');

    std::string packet;
    packet.reserve(3 + crc_input.size() + 2);
    packet.push_back(stx);
    packet.push_back(static_cast<char>(sequence));
    packet.push_back(static_cast<char>(0xffu - sequence));
    packet.append(crc_input);
    const auto packet_crc = crc16_xmodem(crc_input);
    packet.push_back(static_cast<char>(packet_crc >> 8));
    packet.push_back(static_cast<char>(packet_crc & 0xffu));

    for (int attempts = 0; attempts < 16; ++attempts) {
      if (!write_exact(fd, packet.data(), packet.size())) {
        return false;
      }
      for (int responses = 0; responses < 80; ++responses) {
        if (!read_exact_timeout_pumping(fd, &response, 1, std::chrono::milliseconds(500), pump)) {
          break;
        }
        if (response == ack) {
          return true;
        }
        if (response == '\x15') {
          break;
        }
      }
    }
    return false;
  };

  constexpr std::string_view placeholder_md5 = "00000000000000000000000000000000";
  if (!send_packet(0, placeholder_md5) || !send_packet(1, contents) || !write_exact(fd, &eot, 1)) {
    return false;
  }
  return read_exact_timeout_pumping(fd, &response, 1, std::chrono::seconds(2), pump) && response == ack;
}

}  // namespace sim::test

#endif
