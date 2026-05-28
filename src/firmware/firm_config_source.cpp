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

#include "FirmConfigSource.h"

#include <string>

#include "ConfigCache.h"
#include "ConfigValue.h"
#include "libs/Kernel.h"
#include "sim/firm_config_data.hpp"
#include "utils.h"

namespace {

struct ConfigBlob {
  const char* start;
  const char* end;
};

ConfigBlob blob_for_machine_model() {
  if (!sim::firm_config_data::enabled()) {
    return {nullptr, nullptr};
  }

  switch (THEKERNEL->factory_set->MachineModel) {
    case CARVERA_AIR:
      return {reinterpret_cast<const char*>(sim::firm_config_data::config2_default),
              reinterpret_cast<const char*>(sim::firm_config_data::config2_default) +
                  sim::firm_config_data::config2_default_size};
    case CARVERA:
    default:
      return {reinterpret_cast<const char*>(sim::firm_config_data::config_default),
              reinterpret_cast<const char*>(sim::firm_config_data::config_default) +
                  sim::firm_config_data::config_default_size};
  }
}

}  // namespace

FirmConfigSource::FirmConfigSource(const char* name) {
  name_checksum = get_checksum(name);
  const auto blob = blob_for_machine_model();
  start = blob.start;
  end = blob.end;
}

FirmConfigSource::FirmConfigSource(const char* name, const char* start, const char* end) {
  name_checksum = get_checksum(name);
  this->start = start;
  this->end = end;
}

void FirmConfigSource::transfer_values_to_cache(ConfigCache* cache) {
  if (start == nullptr || end == nullptr) {
    return;
  }

  const char* p = start;
  while (p < end) {
    const char* eol = p;
    while (eol < end) {
      if (*eol++ == '\n') {
        break;
      }
    }
    const std::string line(p, eol - p);
    p = eol;
    process_line_from_ascii_config(line, cache);
  }
  process_line_from_ascii_config("sd_ok true", cache);
}

bool FirmConfigSource::is_named(uint16_t check_sum) { return check_sum == name_checksum; }

bool FirmConfigSource::write(std::string, std::string) { return false; }

std::string FirmConfigSource::read(uint16_t check_sums[3]) {
  if (start == nullptr || end == nullptr) {
    return "";
  }

  const char* p = start;
  while (p < end) {
    const char* eol = p;
    while (eol < end) {
      if (*eol++ == '\n') {
        break;
      }
    }
    const std::string line(p, eol - p);
    p = eol;
    const auto value = process_line_from_ascii_config(line, check_sums);
    if (!value.empty()) {
      return value;
    }
  }
  return process_line_from_ascii_config("sd_ok true", check_sums);
}
