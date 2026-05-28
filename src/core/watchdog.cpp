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

#include "sim/watchdog.hpp"

#include "lpc17xx_wdt.h"
#include "sim/interrupt_controller.hpp"
#include "sim/lpc1768.hpp"
#include "sim/system_reset.hpp"

extern "C" void WDT_IRQHandler(void);

namespace sim::watchdog {

void advance_cycles(std::uint32_t cycles) {
  auto& wdt = lpc1768::watchdog();
  if (cycles == 0 || (wdt.WDMOD & WDT_WDMOD_WDEN) == 0 || wdt.WDTV == 0) {
    return;
  }

  if (cycles < wdt.WDTV) {
    wdt.WDTV -= cycles;
    return;
  }

  wdt.WDTV = 0;
  wdt.WDMOD |= WDT_WDMOD_WDTOF | WDT_WDMOD_WDINT;

  if ((wdt.WDMOD & WDT_WDMOD_WDRESET) != 0) {
    system_reset::request();
    return;
  }

  interrupt_controller::active().raise(WDT_IRQn, []() { WDT_IRQHandler(); });
  interrupt_controller::active().dispatch_all();
}

}  // namespace sim::watchdog
