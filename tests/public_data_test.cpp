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

#include <cstdlib>
#include <iostream>

#include "PublicDataRequest.h"
#include "libs/Kernel.h"
#include "libs/PublicData.h"

#include "sim/machine_simulator.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

class DataModule : public Module {
 public:
  void on_get_public_data(void* argument) override {
    auto* request = static_cast<PublicDataRequest*>(argument);

    if (request->starts_with(0x1234) && request->second_element_is(0x0055)) {
      *static_cast<int*>(request->get_data_ptr()) = 42;
      request->set_taken();
      return;
    }

    if (request->starts_with(0x7777)) {
      request->set_data_ptr(&owned_value);
      request->set_taken();
    }
  }

  void on_set_public_data(void* argument) override {
    auto* request = static_cast<PublicDataRequest*>(argument);
    if (request->starts_with(0x00ab) && request->second_element_is(0x00cd) && request->third_element_is(0x00ef)) {
      last_set_value = *static_cast<int*>(request->get_data_ptr());
      request->set_taken();
    }
  }

  int owned_value{77};
  int last_set_value{0};
};

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;
  DataModule module;

  kernel.register_for_event(ON_GET_PUBLIC_DATA, &module);
  kernel.register_for_event(ON_SET_PUBLIC_DATA, &module);

  int caller_storage = 0;
  require(PublicData::get_value(0x1234, 0x0055, &caller_storage),
          "PublicData should route get requests through Kernel events");
  require(caller_storage == 42, "PublicData get should support caller-owned storage");

  int* returned_storage = nullptr;
  require(PublicData::get_value(0x7777, &returned_storage), "PublicData should support callee-owned returned storage");
  require(returned_storage == &module.owned_value, "PublicData should copy returned storage pointers");
  require(*returned_storage == 77, "PublicData returned pointer should reference module-owned data");

  int value = 13;
  require(PublicData::set_value(0x00ab, 0x00cd, 0x00ef, &value),
          "PublicData should route set requests through Kernel events");
  require(module.last_set_value == 13, "PublicData set should pass caller data to the module");

  require(!PublicData::get_value(0x9999, &caller_storage), "PublicData should report unmatched requests as not taken");

  return 0;
}
