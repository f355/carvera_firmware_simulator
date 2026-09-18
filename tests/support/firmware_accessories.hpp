/*
 * This file is part of the Carvera Firmware Simulator.
 *
 * Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 */

#ifndef CARVERA_SIMULATOR_TESTS_SUPPORT_FIRMWARE_ACCESSORIES_HPP
#define CARVERA_SIMULATOR_TESTS_SUPPORT_FIRMWARE_ACCESSORIES_HPP

#include "libs/Kernel.h"
#include "modules/tools/accessories/BedCleaning.h"
#include "modules/tools/accessories/SpindleAccessories.h"

namespace sim::test {

class FirmwareAccessories {
 public:
  explicit FirmwareAccessories(Kernel& kernel) {
    kernel.bed_cleaning = &bed_cleaning;
    kernel.spindle_accessories = &spindle_accessories;
  }

 private:
  BedCleaning bed_cleaning;
  SpindleAccessories spindle_accessories;
};

}  // namespace sim::test

#endif
