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

#include "sim/timer_scheduler.hpp"

#include <algorithm>

#include "LPC17xx.h"
#include "sim/interrupt_controller.hpp"
#include "sim/lpc1768.hpp"
#include "sim/realtime_timer_pacer.hpp"
#include "sim/simulator_context.hpp"
#include "sim/virtual_clock.hpp"
#include "sim/watchdog.hpp"
#include "compat/active_context.hpp"

extern "C" void TIMER0_IRQHandler(void);
extern "C" void TIMER1_IRQHandler(void);
extern "C" void TIMER2_IRQHandler(void);

namespace {

IRQn_Type irq_for_timer(std::uint8_t timer_index) {
  switch (timer_index) {
    case 0:
      return TIMER0_IRQn;
    case 1:
      return TIMER1_IRQn;
    case 2:
      return TIMER2_IRQn;
    case 3:
      return TIMER3_IRQn;
    default:
      return WDT_IRQn;
  }
}

void call_handler(std::uint8_t timer_index) {
  switch (timer_index) {
    case 0:
      TIMER0_IRQHandler();
      break;
    case 1:
      TIMER1_IRQHandler();
      break;
    case 2:
      TIMER2_IRQHandler();
      break;
    default:
      break;
  }
}

}  // namespace

namespace sim {

void TimerScheduler::reset() { clock_cycle_remainder_ = 0; }

bool TimerScheduler::dispatch_match(std::uint8_t timer_index) {
  if (!raise_match(timer_index)) {
    return false;
  }

  interrupt_controller::active().dispatch_all();
  return true;
}

void TimerScheduler::advance_cycles(std::uint32_t cycles) {
  std::uint32_t remaining = cycles;
  while (remaining > 0) {
    const auto next = cycles_to_next_match();
    if (!next.has_value()) {
      realtime_timer_pacer::active().pace_cycles(remaining);
      advance_running_timers(remaining);
      return;
    }

    if (*next > remaining) {
      realtime_timer_pacer::active().pace_cycles(remaining);
      advance_running_timers(remaining);
      return;
    }

    realtime_timer_pacer::active().pace_cycles(*next);
    advance_running_timers(*next);
    remaining -= *next;

    if (!raise_due_matches()) {
      return;
    }
    interrupt_controller::active().dispatch_all();
  }
}

bool TimerScheduler::advance_to_next_match() {
  const auto next = cycles_to_next_match();
  if (!next.has_value()) {
    return false;
  }

  realtime_timer_pacer::active().pace_cycles(*next);
  advance_running_timers(*next);
  if (!raise_due_matches()) {
    return false;
  }
  interrupt_controller::active().dispatch_all();
  return true;
}

bool TimerScheduler::match_interrupt_enabled(std::uint8_t timer_index) const {
  if (timer_index >= timer_count) {
    return false;
  }

  const auto irq = irq_for_timer(timer_index);
  const auto& timer = lpc1768::active().timer(timer_index);
  return lpc1768::irq_globally_enabled() && lpc1768::irq_enabled(irq) && (timer.TCR & 1u) != 0 &&
         (timer.MCR & 1u) != 0 && timer.MR0 != 0;
}

std::optional<std::uint32_t> TimerScheduler::cycles_to_next_match() const {
  std::optional<std::uint32_t> next;
  for (std::uint8_t timer_index = 0; timer_index < timer_count; ++timer_index) {
    if (!match_interrupt_enabled(timer_index)) {
      continue;
    }

    const auto& timer = lpc1768::active().timer(timer_index);
    const auto cycles = timer.TC >= timer.MR0 ? 0 : timer.MR0 - timer.TC;
    if (!next.has_value() || cycles < *next) {
      next = cycles;
    }
  }
  return next;
}

void TimerScheduler::advance_running_timers(std::uint32_t cycles) {
  if (cycles == 0) {
    return;
  }

  watchdog::advance_cycles(cycles);

  const std::uint64_t pclk_hz = std::max<std::uint32_t>(1, SystemCoreClock / 4);
  const std::uint64_t elapsed_cycle_us = clock_cycle_remainder_ + static_cast<std::uint64_t>(cycles) * 1'000'000ULL;
  clock::active().advance_us(elapsed_cycle_us / pclk_hz);
  clock_cycle_remainder_ = elapsed_cycle_us % pclk_hz;

  for (std::uint8_t timer_index = 0; timer_index < timer_count; ++timer_index) {
    auto& timer = lpc1768::timer(timer_index);
    if ((timer.TCR & 1u) != 0) {
      timer.TC += cycles;
    }
  }
}

bool TimerScheduler::raise_due_matches() {
  bool raised = false;
  for (std::uint8_t timer_index = 0; timer_index < timer_count; ++timer_index) {
    const auto& timer = lpc1768::active().timer(timer_index);
    if (match_interrupt_enabled(timer_index) && timer.TC >= timer.MR0) {
      raised = raise_match(timer_index) || raised;
    }
  }
  return raised;
}

bool TimerScheduler::raise_match(std::uint8_t timer_index) {
  if (!match_interrupt_enabled(timer_index)) {
    return false;
  }

  auto& timer = lpc1768::timer(timer_index);
  timer.TC = timer.MR0;
  interrupt_controller::active().raise(irq_for_timer(timer_index), [timer_index]() {
    auto& matched_timer = lpc1768::timer(timer_index);
    matched_timer.TC = matched_timer.MR0;
    call_handler(timer_index);

    if ((matched_timer.MCR & (1u << 1)) != 0) {
      matched_timer.TC = 0;
    }
    if ((matched_timer.MCR & (1u << 2)) != 0) {
      matched_timer.TCR = 0;
    }
  });
  return true;
}

namespace timer_scheduler {

TimerScheduler& active() { return compat::active_context().timer_scheduler(); }

void reset() { active().reset(); }

}  // namespace timer_scheduler

}  // namespace sim
