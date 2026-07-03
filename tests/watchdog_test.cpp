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

#include "test_support.hpp"

#include "libs/Kernel.h"
#include "libs/Watchdog.h"
#include "lpc17xx_wdt.h"
#include "lpc1768_sim.h"
#include "sim/system_reset.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/timer_irq.hpp"

namespace {

using sim::test::require;

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  sim::lpc1768::reset();
  Kernel kernel;

  Watchdog reset_watchdog(4000000, WDT_RESET);
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDEN) != 0, "WDT_RESET should enable the LPC watchdog");
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDRESET) != 0, "WDT_RESET should use reset watchdog mode");
  require(!sim::lpc1768::irq_enabled(WDT_IRQn), "WDT_RESET should not enable the WDT IRQ");

  sim::lpc1768::reset();

  Watchdog watchdog(123456, WDT_MRI);
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDEN) != 0, "Watchdog constructor should enable the LPC watchdog");
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDRESET) == 0, "WDT_MRI should use interrupt-only watchdog mode");
  require(LPC_WDT->WDTC == 123456, "Watchdog constructor should program the timeout");
  require(LPC_WDT->WDTV == 123456, "Watchdog constructor should feed the watchdog after starting it");
  require(sim::lpc1768::irq_enabled(WDT_IRQn), "WDT_MRI should enable the WDT IRQ");
  require(sim::lpc1768::nvic().priority(WDT_IRQn) == 1, "WDT_MRI should set WDT IRQ priority");

  watchdog.on_module_loaded();
  require(kernel.kernel_has_event(ON_IDLE, &watchdog), "Watchdog should register for idle events");

  LPC_WDT->WDTV = 1;
  kernel.call_event(ON_IDLE);
  require(LPC_WDT->WDTV == LPC_WDT->WDTC, "Watchdog idle event should feed the LPC watchdog");

  LPC_WDT->WDMOD |= WDT_WDMOD_WDTOF;
  require(WDT_ReadTimeOutFlag() == SET, "simulated WDT timeout flag should be readable");
  WDT_ClrTimeOutFlag();
  require(WDT_ReadTimeOutFlag() == RESET, "simulated WDT timeout flag should clear");

  sim::lpc1768::reset();
  WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
  WDT_Start(5);
  sim::timer_irq::advance_cycles(4);
  require(WDT_GetCurrentCount() == 1, "simulated WDT should count down while enabled");
  require(WDT_ReadTimeOutFlag() == RESET, "simulated WDT should not time out before the counter reaches zero");

  sim::timer_irq::advance_cycles(1);
  require(WDT_GetCurrentCount() == 0, "simulated WDT timeout should leave the counter at zero");
  require(WDT_ReadTimeOutFlag() == SET, "simulated WDT timeout should set the timeout flag");
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDINT) != 0, "simulated WDT timeout should set the interrupt flag");
  require(sim::system_reset::consume_requested(), "WDT reset mode timeout should request a simulator firmware reset");

  sim::lpc1768::reset();
  Kernel irq_kernel;
  Watchdog interrupt_watchdog(3, WDT_MRI);
  sim::timer_irq::advance_cycles(3);
  require(WDT_GetCurrentCount() == LPC_WDT->WDTC, "WDT IRQ handler should feed the watchdog after interrupt timeout");
  require(WDT_ReadTimeOutFlag() == RESET, "WDT IRQ handler should clear the timeout flag");
  require((LPC_WDT->WDMOD & WDT_WDMOD_WDINT) == 0, "WDT feed should clear the interrupt flag");

  return 0;
}
