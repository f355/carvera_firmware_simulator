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

#include "sim/logging.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace sim::logging {
namespace {

constexpr std::size_t max_logged_bytes = 512;

std::mutex log_mutex;

}  // namespace

std::string elapsed_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::system_clock::to_time_t(now);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &seconds);
#else
  gmtime_r(&seconds, &utc);
#endif

  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << milliseconds << 'Z';
  return out.str();
}

std::string printable_bytes(std::string_view bytes) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  const auto count = std::min(bytes.size(), max_logged_bytes);
  for (std::size_t i = 0; i < count; ++i) {
    const auto ch = static_cast<unsigned char>(bytes[i]);
    switch (ch) {
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (std::isprint(ch) != 0) {
          out << static_cast<char>(ch);
        } else {
          out << "\\x" << std::setw(2) << static_cast<unsigned int>(ch);
        }
        break;
    }
  }
  if (bytes.size() > max_logged_bytes) {
    out << "...(" << std::dec << bytes.size() << " bytes)";
  }
  return out.str();
}

void event(std::string_view category, std::string_view message) {
  std::scoped_lock lock(log_mutex);
  std::cerr << elapsed_timestamp() << " [sim " << category << "] " << message << '\n';
}

void traffic(std::string_view channel, std::string_view direction, std::string_view bytes) {
  if (bytes.empty()) {
    return;
  }
  std::scoped_lock lock(log_mutex);
  std::cerr << elapsed_timestamp() << " [sim " << channel << ' ' << direction << "] " << printable_bytes(bytes) << '\n';
}

}  // namespace sim::logging
