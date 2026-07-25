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

#include <algorithm>
#include <array>
#include <cstdint>

#include "LPC17xx.h"
#include "libs/ADC/adc.h"
#include "sim/adc.hpp"
#include "sim/lpc1768.hpp"
#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace {

constexpr std::uint32_t done_bit = 1u << 31;
constexpr std::uint32_t overrun_bit = 1u << 30;

volatile std::uint32_t& channel_register(std::uint8_t channel) {
  switch (channel) {
    case 0:
      return LPC_ADC->ADDR0;
    case 1:
      return LPC_ADC->ADDR1;
    case 2:
      return LPC_ADC->ADDR2;
    case 3:
      return LPC_ADC->ADDR3;
    case 4:
      return LPC_ADC->ADDR4;
    case 5:
      return LPC_ADC->ADDR5;
    case 6:
      return LPC_ADC->ADDR6;
    default:
      return LPC_ADC->ADDR7;
  }
}

}  // namespace

namespace sim {

std::uint32_t AdcState::data_register(std::uint8_t channel) const {
  return (static_cast<std::uint32_t>(raw_channels_[channel]) << 4) |
         (static_cast<std::uint32_t>(channel & 0x7u) << 24) | done_bit;
}

void AdcState::reset() {
  raw_channels_.fill(0);
  channel_handlers_.fill(nullptr);
  global_handler_ = nullptr;
  active_adc_ = nullptr;
}

void AdcState::set_channel_raw(std::uint8_t channel, std::uint16_t raw12) {
  if (channel >= channel_count) {
    return;
  }

  raw_channels_[channel] = std::min<std::uint16_t>(raw12, 4095);
  publish_sample(channel, AdcState::isr_repeats_per_sample);
}

std::uint16_t AdcState::channel_raw(std::uint8_t channel) const {
  if (channel >= channel_count) {
    return 0;
  }
  return raw_channels_[channel];
}

void AdcState::publish_sample(std::uint8_t channel, int repeats) {
  if (channel >= channel_count) {
    return;
  }

  const auto data = data_register(channel);
  channel_register(channel) = data;
  LPC_ADC->ADGDR = data;
  LPC_ADC->ADSTAT |= static_cast<std::uint32_t>(1u << channel);

  if ((LPC_ADC->ADINTEN & (1u << channel)) == 0 || !sim::lpc1768::irq_enabled(ADC_IRQn)) {
    return;
  }

  for (int i = 0; i < repeats; ++i) {
    if (channel_handlers_[channel] != nullptr) {
      channel_handlers_[channel](data);
    }
    if (global_handler_ != nullptr) {
      global_handler_(channel, data);
    }
  }
}

void AdcState::set_channel_handler(std::uint8_t channel, void (*handler)(std::uint32_t)) {
  if (channel < channel_count) {
    channel_handlers_[channel] = handler;
  }
}

void AdcState::set_global_handler(void (*handler)(int, std::uint32_t)) { global_handler_ = handler; }

namespace adc {

AdcState& active() { return compat::active_context().adc(); }

void reset() { active().reset(); }

void set_channel_raw(std::uint8_t channel, std::uint16_t raw12) { active().set_channel_raw(channel, raw12); }

std::uint16_t channel_raw(std::uint8_t channel) { return active().channel_raw(channel); }

}  // namespace adc

}  // namespace sim

namespace mbed {

ADC* ADC::instance = nullptr;

ADC::ADC(int sample_rate, int cclk_div) {
  instance = this;
  sim::adc::active().set_active_adc(this);
  _adc_g_isr = nullptr;
  _adc_m_isr = nullptr;
  for (std::uint8_t channel = 0; channel < sim::AdcState::channel_count; ++channel) {
    _adc_data[channel] = 0;
    _adc_isr[channel] = nullptr;
  }

  const int adc_clk_freq = CLKS_PER_SAMPLE * sample_rate;
  const int cclk = SystemCoreClock;
  const int pclk = cclk / std::max(cclk_div, 1);
  int clock_div = pclk / std::max(adc_clk_freq, 1);
  clock_div = std::clamp(clock_div, 1, 0xFF);
  _adc_clk_freq = pclk / clock_div;

  LPC_SC->PCONP |= (1u << 12);
  LPC_SC->PCLKSEL0 &= ~(0x3u << 24);
  LPC_ADC->ADCR = (static_cast<std::uint32_t>(clock_div - 1) << 8) | (1u << 21);
  LPC_ADC->ADCR &= ~0xFFu;
  LPC_ADC->ADINTEN &= ~0x100u;
}

int ADC::_pin_to_channel(PinName pin) {
  switch (pin) {
    case p16:
      return 1;
    case p17:
      return 2;
    case p18:
      return 3;
    case p19:
      return 4;
    case p20:
      return 5;
    case p15:
    default:
      return 0;
  }
}

PinName ADC::channel_to_pin(int chan) {
  static const PinName pins[sim::AdcState::channel_count] = {p15, p16, p17, p18, p19, p20, p15, p15};
  return pins[static_cast<std::uint8_t>(chan) & 0x7u];
}

int ADC::channel_to_pin_number(int chan) {
  static const int pins[sim::AdcState::channel_count] = {15, 16, 17, 18, 19, 20, 0, 0};
  return pins[static_cast<std::uint8_t>(chan) & 0x7u];
}

void ADC::setup(PinName pin, int state) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  if ((state & 1) != 0) {
    if (!burst()) {
      LPC_ADC->ADCR &= ~0xFFu;
    }
    LPC_ADC->ADCR |= (1u << channel);
    sim::adc::active().publish_sample(channel, sim::AdcState::isr_repeats_per_sample);
  } else {
    LPC_ADC->ADCR &= ~(1u << channel);
  }
}

int ADC::setup(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  return (LPC_ADC->ADCR >> channel) & 0x1u;
}

void ADC::select(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  if (!burst()) {
    LPC_ADC->ADCR &= ~0xFFu;
  }
  LPC_ADC->ADCR |= (1u << channel);
}

void ADC::burst(int state) {
  if ((state & 1) != 0) {
    LPC_ADC->ADCR |= (1u << 16);
  } else {
    LPC_ADC->ADCR &= ~(1u << 16);
  }
}

int ADC::burst() { return (LPC_ADC->ADCR >> 16) & 0x1u; }

void ADC::startmode(int mode, int edge) {
  LPC_ADC->ADCR = (LPC_ADC->ADCR & ~(0xFu << 24)) | ((static_cast<std::uint32_t>(mode) & 0x7u) << 24) |
                  ((static_cast<std::uint32_t>(edge) & 0x1u) << 27);
}

int ADC::startmode(int mode_edge) {
  if (mode_edge == 1) {
    return (LPC_ADC->ADCR >> 27) & 0x1u;
  }
  return (LPC_ADC->ADCR >> 24) & 0x7u;
}

void ADC::start() { startmode(1, 0); }

void ADC::interrupt_state(PinName pin, int state) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  if (state == 1) {
    LPC_ADC->ADINTEN &= ~0x100u;
    LPC_ADC->ADINTEN |= 1u << channel;
    NVIC_EnableIRQ(ADC_IRQn);
    sim::adc::active().publish_sample(channel, sim::AdcState::isr_repeats_per_sample);
  } else {
    LPC_ADC->ADINTEN &= ~(1u << channel);
    if ((LPC_ADC->ADINTEN & 0xFFu) == 0) {
      NVIC_DisableIRQ(ADC_IRQn);
    }
  }
}

int ADC::interrupt_state(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  return (LPC_ADC->ADINTEN >> channel) & 0x1u;
}

void ADC::attach(void (*fptr)(void)) {
  // TODO: dispatch the legacy global ADC ISR path if a stock module starts
  // using it. Current stock paths attach per-channel handlers via append().
  _adc_m_isr = fptr;
}

void ADC::detach() { _adc_m_isr = nullptr; }

void ADC::append(PinName pin, void (*fptr)(std::uint32_t value)) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  sim::adc::active().set_channel_handler(channel, fptr);
  _adc_isr[channel] = fptr;
}

void ADC::unappend(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  sim::adc::active().set_channel_handler(channel, nullptr);
  _adc_isr[channel] = nullptr;
}

void ADC::append(void (*fptr)(int chan, std::uint32_t value)) {
  sim::adc::active().set_global_handler(fptr);
  _adc_g_isr = fptr;
}

void ADC::unappend() {
  sim::adc::active().set_global_handler(nullptr);
  _adc_g_isr = nullptr;
}

void ADC::offset(int offset) {
  LPC_ADC->ADTRM = (LPC_ADC->ADTRM & ~(0x7u << 4)) | ((static_cast<std::uint32_t>(offset) & 0x7u) << 4);
}

int ADC::offset() { return (LPC_ADC->ADTRM >> 4) & 0x7u; }

int ADC::read(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  sim::adc::active().publish_sample(channel, 1);
  return (channel_register(channel) >> 4) & 0xFFFu;
}

int ADC::done(PinName pin) { return (channel_register(static_cast<std::uint8_t>(_pin_to_channel(pin))) >> 31) & 0x1u; }

int ADC::overrun(PinName pin) {
  return (channel_register(static_cast<std::uint8_t>(_pin_to_channel(pin))) & overrun_bit) != 0;
}

int ADC::actual_adc_clock() { return _adc_clk_freq; }

int ADC::actual_sample_rate() { return _adc_clk_freq / CLKS_PER_SAMPLE; }

void ADC::adcisr() {
  for (std::uint8_t channel = 0; channel < sim::AdcState::channel_count; ++channel) {
    if ((LPC_ADC->ADSTAT & (1u << channel)) != 0) {
      sim::adc::active().publish_sample(channel, 1);
    }
  }
}

void ADC::_adcisr() {
  if (auto* active_adc = sim::adc::active().active_adc()) {
    active_adc->adcisr();
  }
}

std::uint32_t ADC::_data_of_pin(PinName pin) {
  const auto channel = static_cast<std::uint8_t>(_pin_to_channel(pin));
  return channel_register(channel);
}

}  // namespace mbed
