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

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>

#include "checksumm.h"
#include "libs/Kernel.h"
#include "libs/FirmwareFileSystem.h"
#include "sim/delay_hooks.hpp"
#include "sim/host_filesystem.hpp"
#include "sim/system_reset.hpp"
#include "sim/us_ticker_sim.hpp"
#include "sim/virtual_clock.hpp"
#include "utils.h"

extern "C" caddr_t _sbrk(int) { return 0; }

extern "C" {
uint32_t __end__ = 0;
uint32_t __malloc_free_list = 0;
}
unsigned int g_maximumHeapAddress = 0;

namespace {
std::atomic_bool reset_requested{false};
std::atomic<long long> time_offset_seconds{0};

time_t host_epoch_seconds() {
  using namespace std::chrono;
  return static_cast<time_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
}  // namespace

namespace sim::system_reset {

void request() { reset_requested.store(true); }

bool consume_requested() { return reset_requested.exchange(false); }

}  // namespace sim::system_reset

bool is_whitespace(int c) { return c == ' ' || c == '\t'; }

bool is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

bool is_digit(int c) { return c >= '0' && c <= '9'; }

bool is_numeric(int c) { return is_digit(c) || c == '.' || c == '-' || c == 'e'; }

bool is_alphanum(int c) { return is_alpha(c) || is_numeric(c); }

std::string lc(const std::string& str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower;
}

std::string shift_parameter(std::string& parameters) {
  const auto start = parameters.find_first_not_of(" \t");
  if (start == std::string::npos) {
    parameters.clear();
    return "";
  }

  if (parameters[start] == '"' || parameters[start] == '\'') {
    const char quote = parameters[start];
    const auto end_quote = parameters.find(quote, start + 1);
    if (end_quote != std::string::npos) {
      auto value = parameters.substr(start + 1, end_quote - start - 1);
      const auto next = parameters.find_first_not_of(" \t", end_quote + 1);
      parameters = next == std::string::npos ? "" : parameters.substr(next);
      return value;
    }
  }

  const auto end = parameters.find_first_of(" ", start);
  if (end == std::string::npos) {
    const auto value = parameters.substr(start);
    parameters.clear();
    return value;
  }

  const auto value = parameters.substr(start, end - start);
  const auto next = parameters.find_first_not_of(" \t", end);
  parameters = next == std::string::npos ? "" : parameters.substr(next);
  return value;
}

std::string get_arguments(const std::string& possible_command) {
  const auto beginning = possible_command.find_first_of(" \t");
  return beginning == std::string::npos ? "" : possible_command.substr(beginning + 1);
}

void ltrim(std::string& s, const char* t) {
  const auto first = s.find_first_not_of(t);
  if (first == std::string::npos) {
    s.clear();
  } else if (first > 0) {
    s.erase(0, first);
  }
}

const char* ltrim_cstr(const char* s) {
  while (*s != '\0' && std::isspace(static_cast<unsigned char>(*s))) {
    ++s;
  }
  return s;
}

uint16_t get_checksum(const std::string& to_check) { return get_checksum(to_check.c_str()); }

uint16_t get_checksum(const char* to_check) {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  while (*to_check != '\0') {
    sum1 = static_cast<uint16_t>((sum1 + *to_check++) % 255);
    sum2 = static_cast<uint16_t>((sum2 + sum1) % 255);
  }
  return static_cast<uint16_t>((sum2 << 8) | sum1);
}

void get_checksums(uint16_t check_sums[], const std::string& key) {
  size_t begin = 0;
  for (int index = 0; index < 3; ++index) {
    const size_t dot = key.find('.', begin);
    const auto part = key.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin);
    check_sums[index] = part.empty() ? 0 : get_checksum(part);
    if (dot == std::string::npos) {
      for (int fill = index + 1; fill < 3; ++fill) {
        check_sums[fill] = 0;
      }
      return;
    }
    begin = dot + 1;
  }
}

std::string remove_non_number(std::string str) {
  const std::string number_mask = "0123456789-.abcdefpxABCDEFPX";
  auto found = str.find_first_not_of(number_mask);
  while (found != std::string::npos) {
    str.erase(found, 1);
    found = str.find_first_not_of(number_mask);
  }
  return str;
}

std::vector<std::string> split(const char* str, char delimiter) {
  std::vector<std::string> result;
  if (str == nullptr) {
    return result;
  }

  do {
    const char* begin = str;
    while (*str != delimiter && *str != '\0') {
      ++str;
    }
    result.emplace_back(begin, str);
  } while (*str++ != '\0');

  return result;
}

std::vector<float> parse_number_list(const char* str) {
  std::vector<float> values;
  if (str == nullptr) {
    return values;
  }

  for (const auto& token : split(str, ',')) {
    values.push_back(std::strtof(token.c_str(), nullptr));
  }
  return values;
}

std::vector<uint32_t> parse_number_list(const char* str, uint8_t radix) {
  std::vector<uint32_t> values;
  if (str == nullptr) {
    return values;
  }

  for (const auto& token : split(str, ',')) {
    values.push_back(static_cast<uint32_t>(std::strtoul(token.c_str(), nullptr, radix)));
  }
  return values;
}

std::string wcs2gcode(int wcs) {
  std::string code = "G5";
  code.append(1, static_cast<char>(std::min(wcs, 5) + '4'));
  if (wcs >= 6) {
    code.append(".").append(1, static_cast<char>('1' + (wcs - 6)));
  }
  return code;
}

bool file_exists(const std::string file_name) { return sim::host_filesystem::exists(file_name); }

std::string absolute_from_relative(std::string path) {
  std::string cwd = THEKERNEL == nullptr ? "/sd" : THEKERNEL->current_path;
  if (path.empty()) {
    return cwd;
  }
  if (path[0] == '/') {
    return path;
  }

  while (path.rfind("../", 0) == 0) {
    path = path.substr(3);
    const auto slash = cwd.find_last_of('/');
    cwd = slash == std::string::npos ? "" : cwd.substr(0, slash);
  }

  if (path.rfind("..", 0) == 0) {
    path = path.substr(2);
    const auto slash = cwd.find_last_of('/');
    cwd = slash == std::string::npos ? "" : cwd.substr(0, slash);
  }

  return (!cwd.empty() && cwd.back() == '/') ? cwd + path : cwd + '/' + path;
}

std::string change_to_md5_path(std::string origin) {
  const auto found = origin.find("gcodes/");
  const auto filename = found == std::string::npos ? origin : origin.substr(found + 7);
  sim::host_filesystem::ensure_mount("sd");
  fwfs::mkdir("/sd/gcodes", 0);
  fwfs::mkdir("/sd/gcodes/.md5", 0);
  return "/sd/gcodes/.md5/" + filename;
}

std::string change_to_lz_path(std::string origin) {
  const auto found = origin.find("gcodes/");
  const auto filename = found == std::string::npos ? origin : origin.substr(found + 7);
  sim::host_filesystem::ensure_mount("sd");
  fwfs::mkdir("/sd/gcodes", 0);
  fwfs::mkdir("/sd/gcodes/.lz", 0);
  return "/sd/gcodes/.lz/" + filename;
}

void check_and_make_path(std::string origin) {
  std::size_t pos = 0;
  while ((pos = origin.find_first_of('/', pos)) != std::string::npos) {
    const auto directory = origin.substr(0, pos++);
    if (!directory.empty()) {
      fwfs::mkdir(directory.c_str(), 0);
    }
  }
}

int append_parameters(char* buf, std::vector<std::pair<char, float>> params, size_t bufsize) {
  size_t n = 0;
  for (auto& parameter : params) {
    if (n >= bufsize) {
      break;
    }
    buf[n++] = parameter.first;
    n += std::snprintf(&buf[n], bufsize - n, "%1.4f ", parameter.second);
  }
  return static_cast<int>(n);
}

void system_reset(bool) { sim::system_reset::request(); }

namespace {

void advance_delay_time(std::uint64_t delay_us) {
  if (!sim::clock::active().is_realtime()) {
    sim::clock::active().advance_us(delay_us);
    sim::us_ticker::dispatch_due_events();
  }
  if (sim::delay_hooks::run()) {
    const auto wall_delay_us = std::min<std::uint64_t>(delay_us, 1000);
    std::this_thread::sleep_for(std::chrono::microseconds(wall_delay_us));
  }
  if (THEKERNEL != nullptr) {
    THEKERNEL->call_event(ON_IDLE, nullptr);
  }
}

}  // namespace

void safe_delay_ms(uint32_t ms) { advance_delay_time(static_cast<std::uint64_t>(ms) * 1000U); }

void safe_delay_us(uint32_t us) { advance_delay_time(us); }

extern "C" time_t time(time_t* out) {
  const auto current_time = static_cast<time_t>(host_epoch_seconds() + time_offset_seconds.load());
  if (out != nullptr) {
    *out = current_time;
  }
  return current_time;
}

extern "C" void set_time(time_t new_time) {
  time_offset_seconds.store(static_cast<long long>(new_time - host_epoch_seconds()));
}

struct tm* get_fftime(unsigned short t_date, unsigned short t_time, struct tm* timeinfo) {
  if (timeinfo == nullptr) {
    return nullptr;
  }
  std::memset(timeinfo, 0, sizeof(*timeinfo));
  timeinfo->tm_sec = t_time & ((1 << 5) - 1);
  timeinfo->tm_min = (t_time >> 5) & ((1 << 6) - 1);
  timeinfo->tm_hour = (t_time >> 11) & ((1 << 5) - 1);
  timeinfo->tm_mday = t_date & ((1 << 5) - 1);
  timeinfo->tm_mon = (t_date >> 5) & ((1 << 4) - 1);
  timeinfo->tm_year = (t_date >> 9) & ((1 << 7) - 1);
  return timeinfo;
}
