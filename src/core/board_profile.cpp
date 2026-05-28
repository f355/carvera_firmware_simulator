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

#include "sim/board_profile.hpp"

namespace sim {

namespace {

constexpr Box c1_physical_travel{
    -372.0, -251.0, -136.0, 1.0, 1.0, 1.0,
};

constexpr Box c1_tool_setter{
    -10.158, -60.568, -107.5, 1.842, -48.568, -105.5,
};

constexpr Box ca1_physical_travel{
    -303.0, -213.0, -122.0, 1.0, 1.0, 1.0,
};

constexpr Box ca1_tool_setter{
    -17.0, -13.0, -117.0, -5.0, -1.0, -115.0,
};

BoardSignal signal(PinAddress pin, bool active_level = true) { return BoardSignal{pin, active_level, true}; }

const BoardProfile& c1_profile() {
  static const BoardProfile profile = [] {
    BoardProfile profile;
    profile.model = MachineModel::CarveraC1;
    profile.name = "C1";
    profile.default_main_button_pin = "1.16^";
    profile.default_e_stop_pin = "0.26^";
    profile.default_cover_pin = "1.9^";
    profile.default_main_button_led_r_pin = "1.10";
    profile.default_main_button_led_g_pin = "1.15";
    profile.default_main_button_led_b_pin = "1.14";
    profile.physical_main_button = signal({1, 16});
    profile.physical_e_stop = signal({0, 26}, false);
    profile.physical_probe = signal({2, 6});
    profile.physical_tool_setter = signal({0, 5});
    profile.physical_atc_detector = signal({0, 20});
    profile.physical_atc_home = signal({1, 0});
    profile.spindle_fan_output = signal({2, 1});
    profile.physical_limits = {{
        {signal({0, 24}), signal({0, 25})},
        {signal({1, 1}), signal({1, 4})},
        {{}, signal({1, 8})},
    }};
    profile.geometry = MachineGeometry{
        c1_physical_travel,
        c1_tool_setter,
        {{
            {-10.0, c1_physical_travel.min_x, c1_physical_travel.max_x},
            {-10.0, c1_physical_travel.min_y, c1_physical_travel.max_y},
            {-10.0, c1_physical_travel.min_z, c1_physical_travel.max_z},
        }},
    };
    return profile;
  }();
  return profile;
}

const BoardProfile& ca1_profile() {
  static const BoardProfile profile = [] {
    BoardProfile profile;
    profile.model = MachineModel::CarveraAirCA1;
    profile.name = "CA1";
    profile.default_main_button_pin = "2.13!^";
    profile.default_e_stop_pin = "0.20^";
    profile.default_cover_pin = "1.8!^";
    profile.default_main_button_led_r_pin = "nc";
    profile.default_main_button_led_g_pin = "1.15";
    profile.default_main_button_led_b_pin = "nc";
    profile.physical_main_button = signal({2, 13}, false);
    profile.physical_e_stop = signal({0, 20});
    profile.physical_probe = signal({2, 6});
    profile.physical_tool_setter = signal({0, 5});
    profile.spindle_fan_output = signal({2, 1});
    profile.power_fan_output = signal({2, 3});
    profile.physical_limits = {{
        {{}, signal({0, 24})},
        {{}, signal({0, 25})},
        {{}, signal({1, 1})},
    }};
    profile.geometry = MachineGeometry{
        ca1_physical_travel,
        ca1_tool_setter,
        {{
            {-10.0, ca1_physical_travel.min_x, ca1_physical_travel.max_x},
            {-10.0, ca1_physical_travel.min_y, ca1_physical_travel.max_y},
            {-10.0, ca1_physical_travel.min_z, ca1_physical_travel.max_z},
        }},
    };
    return profile;
  }();
  return profile;
}

}  // namespace

const BoardProfile& board_profile(MachineModel model) {
  if (model == MachineModel::CarveraAirCA1) {
    return ca1_profile();
  }
  return c1_profile();
}

bool signal_level(const BoardSignal& signal, bool active) {
  return active ? signal.active_level : !signal.active_level;
}

}  // namespace sim
