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

#ifndef SIMULATOR_SIM_ADC_HPP
#define SIMULATOR_SIM_ADC_HPP

#include <array>
#include <cstdint>

namespace mbed {
class ADC;
}

namespace sim {

class AdcState {
 public:
  static constexpr std::uint8_t channel_count = 8;

  void reset();
  void set_channel_raw(std::uint8_t channel, std::uint16_t raw12);
  std::uint16_t channel_raw(std::uint8_t channel) const;
  void publish_sample(std::uint8_t channel, int repeats);
  void set_channel_handler(std::uint8_t channel, void (*handler)(std::uint32_t));
  void set_global_handler(void (*handler)(int, std::uint32_t));
  void set_active_adc(mbed::ADC* adc) { active_adc_ = adc; }
  mbed::ADC* active_adc() const { return active_adc_; }

 private:
  std::uint32_t data_register(std::uint8_t channel) const;

  std::array<std::uint16_t, channel_count> raw_channels_{};
  std::array<void (*)(std::uint32_t), channel_count> channel_handlers_{};
  void (*global_handler_)(int, std::uint32_t) = nullptr;
  mbed::ADC* active_adc_ = nullptr;
};

namespace adc {

AdcState& active();
void reset();
void set_channel_raw(std::uint8_t channel, std::uint16_t raw12);
std::uint16_t channel_raw(std::uint8_t channel);

}  // namespace adc

}  // namespace sim

#endif
