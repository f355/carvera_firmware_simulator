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

#ifndef SIMULATOR_SIM_RUNTIME_CHECKSUMS_HPP
#define SIMULATOR_SIM_RUNTIME_CHECKSUMS_HPP

#include <cstddef>
#include <cstdint>

#include "checksumm.h"

namespace sim::runtime_checksums {

inline constexpr std::uint16_t cover_endstop = CHECKSUM("cover_endstop");
inline constexpr std::uint16_t main_button_pin = CHECKSUM("main_button_pin");
inline constexpr std::uint16_t main_button_led_r_pin = CHECKSUM("main_button_LED_R_pin");
inline constexpr std::uint16_t main_button_led_g_pin = CHECKSUM("main_button_LED_G_pin");
inline constexpr std::uint16_t main_button_led_b_pin = CHECKSUM("main_button_LED_B_pin");
inline constexpr std::uint16_t e_stop_pin = CHECKSUM("e_stop_pin");
inline constexpr std::uint16_t watchdog_timeout = CHECKSUM("watchdog_timeout");
inline constexpr std::uint16_t spindle = CHECKSUM("spindle");
inline constexpr std::uint16_t spindle_pwm_pin = CHECKSUM("pwm_pin");
inline constexpr std::uint16_t spindle_max_pwm = CHECKSUM("max_pwm");
inline constexpr std::uint16_t spindle_feedback_pin = CHECKSUM("feedback_pin");
inline constexpr std::uint16_t spindle_pulses_per_rev = CHECKSUM("pulses_per_rev");
inline constexpr std::uint16_t spindle_acc_ratio = CHECKSUM("acc_ratio");
inline constexpr std::uint16_t spindle_alarm_pin = CHECKSUM("alarm_pin");

inline constexpr std::uint16_t limit_min[] = {
    CHECKSUM("alpha_min_endstop"), CHECKSUM("beta_min_endstop"),    CHECKSUM("gamma_min_endstop"),
    CHECKSUM("delta_min_endstop"), CHECKSUM("epsilon_min_endstop"),
};

inline constexpr std::uint16_t limit_max[] = {
    CHECKSUM("alpha_max_endstop"), CHECKSUM("beta_max_endstop"),    CHECKSUM("gamma_max_endstop"),
    CHECKSUM("delta_max_endstop"), CHECKSUM("epsilon_max_endstop"),
};

inline constexpr std::uint16_t motor_alarm[] = {
    CHECKSUM("alpha_motor_alarm_pin"), CHECKSUM("beta_motor_alarm_pin"),    CHECKSUM("gamma_motor_alarm_pin"),
    CHECKSUM("delta_motor_alarm_pin"), CHECKSUM("epsilon_motor_alarm_pin"),
};
inline constexpr std::size_t motor_alarm_count = sizeof(motor_alarm) / sizeof(motor_alarm[0]);

}  // namespace sim::runtime_checksums

#endif
