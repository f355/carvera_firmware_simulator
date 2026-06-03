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

#include "lpc1768_sim.h"

#include "lpc17xx_wdt.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "sim/simulator_context.hpp"
#include "sim/stepper_axis.hpp"

uint32_t SystemCoreClock = 100000000;

namespace {

void notify_gpio_changes(std::uint8_t port, std::uint32_t previous, std::uint32_t current) {
  const auto changed = previous ^ current;
  for (std::uint8_t pin = 0; pin < 32; ++pin) {
    const auto mask = static_cast<std::uint32_t>(1u << pin);
    if ((changed & mask) != 0) {
      sim::stepper_axes::on_gpio_level_changed({port, pin}, (current & mask) != 0);
    }
  }
}

volatile std::uint32_t* pin_function_register(LPC_PINCON_TypeDef& pincon, std::uint8_t index) {
  switch (index) {
    case 0:
      return &pincon.PINSEL0;
    case 1:
      return &pincon.PINSEL1;
    case 2:
      return &pincon.PINSEL2;
    case 3:
      return &pincon.PINSEL3;
    case 4:
      return &pincon.PINSEL4;
    case 5:
      return &pincon.PINSEL5;
    case 6:
      return &pincon.PINSEL6;
    case 7:
      return &pincon.PINSEL7;
    case 8:
      return &pincon.PINSEL8;
    case 9:
      return &pincon.PINSEL9;
    case 10:
      return &pincon.PINSEL10;
    default:
      return nullptr;
  }
}

volatile std::uint32_t* pin_mode_register(LPC_PINCON_TypeDef& pincon, std::uint8_t index) {
  switch (index) {
    case 0:
      return &pincon.PINMODE0;
    case 1:
      return &pincon.PINMODE1;
    case 2:
      return &pincon.PINMODE2;
    case 3:
      return &pincon.PINMODE3;
    case 4:
      return &pincon.PINMODE4;
    case 5:
      return &pincon.PINMODE5;
    case 6:
      return &pincon.PINMODE6;
    case 7:
      return &pincon.PINMODE7;
    case 8:
      return &pincon.PINMODE8;
    case 9:
      return &pincon.PINMODE9;
    default:
      return nullptr;
  }
}

volatile std::uint32_t* pin_open_drain_register(LPC_PINCON_TypeDef& pincon, std::uint8_t port) {
  switch (port) {
    case 0:
      return &pincon.PINMODE_OD0;
    case 1:
      return &pincon.PINMODE_OD1;
    case 2:
      return &pincon.PINMODE_OD2;
    case 3:
      return &pincon.PINMODE_OD3;
    case 4:
      return &pincon.PINMODE_OD4;
    default:
      return nullptr;
  }
}

void write_two_bit_pin_field(volatile std::uint32_t& reg, std::uint8_t pin, std::uint8_t value) {
  const auto shift = static_cast<std::uint32_t>((pin % 16) * 2);
  const auto mask = static_cast<std::uint32_t>(0x3u << shift);
  reg = static_cast<std::uint32_t>((reg & ~mask) | ((value & 0x3u) << shift));
}

}  // namespace

void LPC_TIM_ControlRegister::bind(LPC_TIM_TypeDef* timer) { owner = timer; }

LPC_TIM_ControlRegister& LPC_TIM_ControlRegister::operator=(uint32_t bits) {
  value = bits;
  if (owner != nullptr && (bits & (1u << 1)) != 0) {
    owner->TC = 0;
    owner->PC = 0;
  }
  return *this;
}

LPC_TIM_ControlRegister& LPC_TIM_ControlRegister::operator|=(uint32_t bits) { return *this = (value | bits); }

void LPC_GPIO_WriteRegister::bind(LPC_GPIO_TypeDef* gpio, uint8_t port, bool set_register) {
  owner = gpio;
  port_number = port;
  sets_bits = set_register;
}

LPC_GPIO_WriteRegister& LPC_GPIO_WriteRegister::operator=(uint32_t bits) {
  value = bits;

  if (owner != nullptr) {
    const auto previous = owner->FIOPIN;
    const auto driven_bits = bits & owner->FIODIR;
    if (sets_bits) {
      owner->FIOPIN |= driven_bits;
    } else {
      owner->FIOPIN &= ~driven_bits;
    }
    notify_gpio_changes(port_number, previous, owner->FIOPIN);
  }

  return *this;
}

LPC_GPIO_WriteRegister& LPC_GPIO_WriteRegister::operator|=(uint32_t bits) {
  value |= bits;

  if (owner != nullptr) {
    const auto previous = owner->FIOPIN;
    const auto driven_bits = bits & owner->FIODIR;
    if (sets_bits) {
      owner->FIOPIN |= driven_bits;
    } else {
      owner->FIOPIN &= ~driven_bits;
    }
    notify_gpio_changes(port_number, previous, owner->FIOPIN);
  }

  return *this;
}

namespace sim {

std::size_t Nvic::index(IRQn_Type irq) { return static_cast<std::size_t>(static_cast<int>(irq) + 16); }

bool Nvic::valid(IRQn_Type irq) { return index(irq) < irq_slots; }

void Nvic::reset() {
  enabled_irqs_.fill(false);
  priorities_.fill(0);
  global_irq_enabled_ = true;
  priority_grouping_ = 0;
}

void Nvic::enable(IRQn_Type irq) {
  if (valid(irq)) {
    enabled_irqs_[index(irq)] = true;
  }
}

void Nvic::disable(IRQn_Type irq) {
  if (valid(irq)) {
    enabled_irqs_[index(irq)] = false;
  }
}

bool Nvic::enabled(IRQn_Type irq) const { return valid(irq) && enabled_irqs_[index(irq)]; }

void Nvic::set_priority(IRQn_Type irq, std::uint32_t priority) {
  if (valid(irq)) {
    priorities_[index(irq)] = priority;
  }
}

std::uint32_t Nvic::priority(IRQn_Type irq) const {
  if (valid(irq)) {
    return priorities_[index(irq)];
  }

  return 0;
}

void Nvic::set_priority_grouping(std::uint32_t grouping) { priority_grouping_ = grouping; }

void Nvic::disable_global_irq() { global_irq_enabled_ = false; }

void Nvic::enable_global_irq() { global_irq_enabled_ = true; }

void GpioBlock::reset() {
  for (std::uint8_t port = 0; port < ports_.size(); ++port) {
    auto& gpio = ports_[port];
    gpio = {};
    gpio.FIOSET.bind(&gpio, port, true);
    gpio.FIOCLR.bind(&gpio, port, false);
  }
}

LPC_GPIO_TypeDef* GpioBlock::find(std::uint8_t port) {
  if (port >= ports_.size()) {
    return nullptr;
  }

  return &ports_[port];
}

LPC_GPIO_TypeDef& GpioBlock::port(std::uint8_t port) { return ports_.at(port); }

const LPC_GPIO_TypeDef& GpioBlock::port(std::uint8_t port) const { return ports_.at(port); }

Lpc1768::Lpc1768() { reset(); }

void Lpc1768::reset() {
  SystemCoreClock = 100000000;
  sc_ = {};
  timers_ = {};
  for (auto& timer : timers_) {
    timer.TCR.bind(&timer);
  }
  gpio_.reset();
  pincon_ = {};
  adc_ = {};
  watchdog_ = {};
  nvic_.reset();
}

namespace lpc1768 {

Lpc1768& active() { return simulator_context::active().mcu(); }

void reset() { active().reset(); }

LPC_SC_TypeDef& sc() { return active().sc(); }

LPC_TIM_TypeDef& timer(std::uint8_t timer) { return active().timer(timer); }

LPC_GPIO_TypeDef& gpio_port(std::uint8_t port) { return active().gpio_port(port); }

LPC_PINCON_TypeDef& pincon() { return active().pincon(); }

LPC_ADC_TypeDef& adc() { return active().adc(); }

LPC_WDT_TypeDef& watchdog() { return active().watchdog(); }

Nvic& nvic() { return active().nvic(); }

bool irq_enabled(IRQn_Type irq) { return active().nvic().enabled(irq); }

bool irq_globally_enabled() { return active().nvic().global_irq_enabled(); }

}  // namespace lpc1768

}  // namespace sim

extern "C" {

void FIO_SetDir(uint8_t portNum, uint32_t bitValue, uint8_t dir) {
  if (auto* port = sim::lpc1768::active().find_gpio_port(portNum)) {
    if (dir) {
      port->FIODIR |= bitValue;
    } else {
      port->FIODIR &= ~bitValue;
    }
  }
}

void FIO_SetValue(uint8_t portNum, uint32_t bitValue) {
  if (auto* port = sim::lpc1768::active().find_gpio_port(portNum)) {
    port->FIOSET |= bitValue;
  }
}

void FIO_ClearValue(uint8_t portNum, uint32_t bitValue) {
  if (auto* port = sim::lpc1768::active().find_gpio_port(portNum)) {
    port->FIOCLR |= bitValue;
  }
}

uint32_t FIO_ReadValue(uint8_t portNum) {
  if (auto* port = sim::lpc1768::active().find_gpio_port(portNum)) {
    return port->FIOPIN;
  }

  return 0;
}

void GPIO_SetDir(uint8_t portNum, uint32_t bitValue, uint8_t dir) { FIO_SetDir(portNum, bitValue, dir); }

void GPIO_SetValue(uint8_t portNum, uint32_t bitValue) { FIO_SetValue(portNum, bitValue); }

void GPIO_ClearValue(uint8_t portNum, uint32_t bitValue) { FIO_ClearValue(portNum, bitValue); }

uint32_t GPIO_ReadValue(uint8_t portNum) { return FIO_ReadValue(portNum); }

void PINSEL_ConfigPin(PINSEL_CFG_Type* PinCfg) {
  if (PinCfg == nullptr || PinCfg->Portnum > 4 || PinCfg->Pinnum > 31) {
    return;
  }

  auto& pincon = sim::lpc1768::active().pincon();
  const auto bank = static_cast<std::uint8_t>((PinCfg->Portnum * 2) + (PinCfg->Pinnum / 16));

  if (auto* func_reg = pin_function_register(pincon, bank)) {
    write_two_bit_pin_field(*func_reg, PinCfg->Pinnum, PinCfg->Funcnum);
  }

  if (auto* mode_reg = pin_mode_register(pincon, bank)) {
    write_two_bit_pin_field(*mode_reg, PinCfg->Pinnum, PinCfg->Pinmode);
  }

  if (auto* open_drain_reg = pin_open_drain_register(pincon, PinCfg->Portnum)) {
    const auto mask = static_cast<std::uint32_t>(1u << PinCfg->Pinnum);
    if (PinCfg->OpenDrain == PINSEL_PINMODE_OPENDRAIN) {
      *open_drain_reg |= mask;
    } else {
      *open_drain_reg &= ~mask;
    }
  }
}

void NVIC_EnableIRQ(IRQn_Type IRQn) { sim::lpc1768::active().nvic().enable(IRQn); }

void NVIC_DisableIRQ(IRQn_Type IRQn) { sim::lpc1768::active().nvic().disable(IRQn); }

void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) { sim::lpc1768::active().nvic().set_priority(IRQn, priority); }

uint32_t NVIC_GetPriority(IRQn_Type IRQn) { return sim::lpc1768::active().nvic().priority(IRQn); }

void NVIC_SetPriorityGrouping(uint32_t grouping) { sim::lpc1768::active().nvic().set_priority_grouping(grouping); }

void __disable_irq(void) { sim::lpc1768::active().nvic().disable_global_irq(); }

void __enable_irq(void) { sim::lpc1768::active().nvic().enable_global_irq(); }

void WDT_Init(WDT_CLK_OPT ClkSrc, WDT_MODE_OPT WDTMode) {
  LPC_WDT->WDCLKSEL = static_cast<uint32_t>(ClkSrc);
  if (WDTMode == WDT_MODE_RESET) {
    LPC_WDT->WDMOD |= WDT_WDMOD_WDRESET;
  } else {
    LPC_WDT->WDMOD &= ~WDT_WDMOD_WDRESET;
  }
}

void WDT_Start(uint32_t TimeOut) {
  LPC_WDT->WDTC = TimeOut;
  LPC_WDT->WDTV = TimeOut;
  LPC_WDT->WDMOD |= WDT_WDMOD_WDEN;
  WDT_Feed();
}

void WDT_Feed(void) {
  LPC_WDT->WDFEED = 0x55;
  LPC_WDT->WDTV = LPC_WDT->WDTC;
  LPC_WDT->WDMOD &= ~WDT_WDMOD_WDINT;
}

void WDT_UpdateTimeOut(uint32_t TimeOut) {
  LPC_WDT->WDTC = TimeOut;
  WDT_Feed();
}

FlagStatus WDT_ReadTimeOutFlag(void) { return (LPC_WDT->WDMOD & WDT_WDMOD_WDTOF) != 0 ? SET : RESET; }

void WDT_ClrTimeOutFlag(void) { LPC_WDT->WDMOD &= ~WDT_WDMOD_WDTOF; }

uint32_t WDT_GetCurrentCount(void) { return LPC_WDT->WDTV; }
}
