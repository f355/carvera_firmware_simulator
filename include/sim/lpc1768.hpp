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

#ifndef SIMULATOR_SIM_LPC1768_HPP
#define SIMULATOR_SIM_LPC1768_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "LPC17xx.h"

namespace sim {

class Nvic {
 public:
  void reset();

  void enable(IRQn_Type irq);
  void disable(IRQn_Type irq);
  bool enabled(IRQn_Type irq) const;

  void set_priority(IRQn_Type irq, std::uint32_t priority);
  std::uint32_t priority(IRQn_Type irq) const;

  void set_priority_grouping(std::uint32_t grouping);
  std::uint32_t priority_grouping() const { return priority_grouping_; }

  void disable_global_irq();
  void enable_global_irq();
  bool global_irq_enabled() const { return global_irq_enabled_; }

 private:
  static constexpr std::size_t irq_slots = 64;

  static std::size_t index(IRQn_Type irq);
  static bool valid(IRQn_Type irq);

  std::array<bool, irq_slots> enabled_irqs_{};
  std::array<std::uint32_t, irq_slots> priorities_{};
  bool global_irq_enabled_{true};
  std::uint32_t priority_grouping_{0};
};

class GpioBlock {
 public:
  void reset();

  LPC_GPIO_TypeDef* find(std::uint8_t port);
  LPC_GPIO_TypeDef& port(std::uint8_t port);
  const LPC_GPIO_TypeDef& port(std::uint8_t port) const;

 private:
  std::array<LPC_GPIO_TypeDef, 5> ports_{};
};

class Lpc1768 {
 public:
  Lpc1768();

  void reset();

  LPC_SC_TypeDef& sc() { return sc_; }
  LPC_TIM_TypeDef& timer(std::uint8_t timer) { return timers_[timer]; }
  LPC_GPIO_TypeDef& gpio_port(std::uint8_t port) { return gpio_.port(port); }
  LPC_GPIO_TypeDef* find_gpio_port(std::uint8_t port) { return gpio_.find(port); }
  LPC_PINCON_TypeDef& pincon() { return pincon_; }
  LPC_ADC_TypeDef& adc() { return adc_; }
  LPC_WDT_TypeDef& watchdog() { return watchdog_; }
  Nvic& nvic() { return nvic_; }

 private:
  LPC_SC_TypeDef sc_{};
  std::array<LPC_TIM_TypeDef, 4> timers_{};
  GpioBlock gpio_{};
  LPC_PINCON_TypeDef pincon_{};
  LPC_ADC_TypeDef adc_{};
  LPC_WDT_TypeDef watchdog_{};
  Nvic nvic_{};
};

namespace lpc1768 {

Lpc1768& active();
void reset();
LPC_SC_TypeDef& sc();
LPC_TIM_TypeDef& timer(std::uint8_t timer);
LPC_GPIO_TypeDef& gpio_port(std::uint8_t port);
LPC_PINCON_TypeDef& pincon();
LPC_ADC_TypeDef& adc();
LPC_WDT_TypeDef& watchdog();
Nvic& nvic();
bool irq_enabled(IRQn_Type irq);
bool irq_globally_enabled();

}  // namespace lpc1768

}  // namespace sim

#endif
