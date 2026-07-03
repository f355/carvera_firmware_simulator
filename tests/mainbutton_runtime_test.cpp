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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "PlayerPublicAccess.h"
#include "PublicData.h"
#include "MainButtonPublicAccess.h"
#include "checksumm.h"
#include "libs/Kernel.h"
#include "sim/simulation_instance.hpp"
#include "support/cartesian_config.hpp"
#include "support/temp_sdcard.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

sim::RuntimePumpOptions button_scan_options() {
  sim::RuntimePumpOptions options;
  options.main_loop_iterations = 64;
  options.max_timer_events = 20'000;
  options.timer_budget_policy = sim::TimerBudgetMode::SpendFullBudget;
  return options;
}

bool press_front_button(sim::FirmwareRuntime& runtime, int held_pumps = 1) {
  const auto options = button_scan_options();
  bool reset_requested = false;
  runtime.set_main_button_pressed(true);
  for (int i = 0; i < held_pumps; ++i) {
    reset_requested = runtime.pump(options).reset_requested || reset_requested;
  }
  runtime.set_main_button_pressed(false);
  reset_requested = runtime.pump(options).reset_requested || reset_requested;
  return reset_requested;
}

bool player_is_playing() {
  void* value = nullptr;
  return PublicData::get_value(player_checksum, is_playing_checksum, &value) && value != nullptr &&
         *static_cast<bool*>(value);
}

void write_config(const std::filesystem::path& root, const char* long_press_mode, int long_press_ms) {
  sim::test::CartesianConfigOptions config;
  config.extra =
      "alpha_limit_enable true\n"
      "alpha_motor_alarm_pin 0.1!^\n"
      "beta_limit_enable true\n"
      "gamma_limit_enable true\n"
      "main_button_pin 1.16^\n"
      "main_button_LED_R_pin 1.10\n"
      "main_button_LED_G_pin 1.15\n"
      "main_button_LED_B_pin 1.14\n"
      "main_button_poll_frequency 20\n"
      "main_button_long_press_time " +
      std::to_string(long_press_ms) +
      "\n"
      "main_button_long_press_enable " +
      long_press_mode +
      "\n"
      "e_stop_pin 0.26!^\n"
      "ps12_pin 0.22\n"
      "ps24_pin 0.10\n"
      "power.auto_sleep false\n"
      "switch.light.startup_state false\n"
      "light.turn_off_min 0\n";
  sim::test::write_cartesian_config(root, config);
}

}  // namespace

int main() {
  sim::test::TempDirectory root_dir("carvera_sim_mainbutton_runtime_test");
  const auto& root = root_dir.path();
  write_config(root, "None", 3000);
  sim::SimulationInstance simulation(sim::test::persistent_sd_config(root));
  auto& runtime = simulation.firmware();
  auto& kernel = runtime.boot();

  auto panel = runtime.front_panel_state();
  require(!panel.main_button_pressed, "runtime MainButton should start with the front button released");
  require(!panel.e_stop_pressed, "runtime MainButton should start with e-stop released");
  require(panel.power_rails.v12, "runtime MainButton should turn on the 12V rail");
  require(panel.power_rails.v24, "runtime MainButton should turn on the 24V rail");
  require(panel.direct_rgb.available, "C1 direct RGB LED pins should be readable through front-panel state");

  runtime.set_main_button_pressed(true);
  require(runtime.front_panel_state().main_button_pressed, "front button API should drive the configured firmware pin");
  runtime.set_main_button_pressed(false);
  require(!runtime.front_panel_state().main_button_pressed,
          "front button API should release the configured firmware pin");

  press_front_button(runtime);
  require(kernel.is_halted(), "short front-button press should halt an idle machine through real MainButton logic");
  require(kernel.get_halt_reason() == MANUAL, "short front-button press should report a manual halt");
  kernel.call_event(ON_HALT, reinterpret_cast<void*>(1));
  require(!kernel.is_halted(), "test should clear manual halt before checking e-stop");

  kernel.set_feed_hold(true);
  press_front_button(runtime);
  require(!kernel.get_feed_hold(), "short front-button press should resume firmware feed hold");

  runtime.set_e_stop_pressed(true);
  require(runtime.front_panel_state().e_stop_pressed, "e-stop API should drive the configured firmware pin");
  runtime.run_main_loop(1);
  require(kernel.is_halted(), "real runtime MainButton should halt firmware when e-stop is pressed");
  require(kernel.get_halt_reason() == E_STOP, "runtime MainButton should report E_STOP as the halt reason");

  runtime.write_serial("M999\n");
  runtime.run_until_idle(20'000);
  require(kernel.is_halted(), "M999 should not clear the alarm while the physical e-stop remains pressed");
  runtime.set_e_stop_pressed(false);
  runtime.write_serial("M999\n");
  runtime.run_until_idle(20'000);
  require(!kernel.is_halted(), "M999 should clear the e-stop alarm after the physical switch is released");

  sim::test::TempDirectory ca1_led_dir("carvera_sim_ca1_led_strip_test");
  const auto& ca1_led_root = ca1_led_dir.path();
  sim::test::CartesianConfigOptions ca1_led_config;
  ca1_led_config.extra =
      "main_button_pin 2.13!^\n"
      "main_button_LED_R_pin nc\n"
      "main_button_LED_G_pin 1.15\n"
      "main_button_LED_B_pin nc\n"
      "main_button_poll_frequency 20\n"
      "main_button_long_press_time 3000\n"
      "main_button_long_press_enable None\n"
      "e_stop_pin 0.20^\n"
      "ps12_pin 0.22\n"
      "ps24_pin 0.10\n"
      "power.auto_sleep false\n";
  sim::test::write_cartesian_config(ca1_led_root, ca1_led_config);
  sim::SimulationInstance ca1_led_simulation(sim::test::persistent_sd_config(ca1_led_root));
  auto& ca1_led_runtime = ca1_led_simulation.firmware();
  require(ca1_led_runtime.set_factory_settings({sim::MachineModel::CarveraAirCA1, 2}),
          "CA1 LED strip test should configure factory settings");
  ca1_led_runtime.boot();
  led_rgb colors{12, 34, 56};
  require(PublicData::set_value(main_button_checksum, set_led_bar_checksum, &colors),
          "MainButton should accept LED bar PublicData writes");
  const auto ca1_led_panel = ca1_led_runtime.front_panel_state();
  require(ca1_led_panel.led_strip.available, "CA1 LED strip should be decoded into front-panel state");
  for (const auto& segment : ca1_led_panel.led_strip.segments) {
    require(segment.red == 12 && segment.green == 34 && segment.blue == 56,
            "CA1 LED strip segment should preserve the firmware-programmed RGB value");
  }

  sim::test::TempDirectory alarm_dir("carvera_sim_mainbutton_alarm_test");
  const auto& alarm_root = alarm_dir.path();
  write_config(alarm_root, "None", 1);
  sim::SimulationInstance alarm_simulation(sim::test::persistent_sd_config(alarm_root));
  auto& alarm_runtime = alarm_simulation.firmware();
  auto& alarm_kernel = alarm_runtime.boot();

  alarm_kernel.set_halt_reason(MANUAL);
  alarm_kernel.call_event(ON_HALT, nullptr);
  require(alarm_kernel.is_halted(), "test should start from a manual alarm");
  press_front_button(alarm_runtime, 2);
  require(!alarm_kernel.is_halted(),
          "long front-button press should unlock a manual halt through real MainButton logic");

  alarm_runtime.set_motor_alarm(0, true);
  alarm_runtime.run_main_loop(1);
  require(alarm_kernel.is_halted(), "configured motor alarm should halt before front-button reset");
  require(alarm_kernel.get_halt_reason() == MOTOR_ERROR_X, "configured motor alarm should report a device alarm");
  const bool reset_requested = press_front_button(alarm_runtime, 2);
  require(reset_requested, "long front-button press should request a firmware reset for device alarms");
  auto& rebooted_alarm_kernel = alarm_runtime.boot();
  require(!rebooted_alarm_kernel.is_halted(), "runtime should reboot cleanly after front-button alarm reset");

  sim::test::TempDirectory repeat_dir("carvera_sim_mainbutton_repeat_test");
  const auto& repeat_root = repeat_dir.path();
  write_config(repeat_root, "Repeat", 1);
  std::filesystem::create_directories(repeat_root / "gcodes");
  {
    std::ofstream job(repeat_root / "gcodes" / "repeat.cnc");
    for (int i = 0; i < 50; ++i) {
      job << "G4 P0\n";
    }
  }
  sim::SimulationInstance repeat_simulation(sim::test::persistent_sd_config(repeat_root));
  auto& repeat_runtime = repeat_simulation.firmware();
  auto& repeat_kernel = repeat_runtime.boot();
  repeat_runtime.write_serial("play /sd/gcodes/repeat.cnc\n");
  repeat_runtime.run_main_loop(8);
  require(player_is_playing(), "test job should be playing before aborting");
  repeat_runtime.write_serial("abort\n");
  repeat_runtime.run_until_idle(20'000);
  require(!player_is_playing(), "test job should be stopped before front-button repeat");
  press_front_button(repeat_runtime, 2);
  require(!repeat_kernel.is_halted(), "front-button repeat should not halt an idle machine");
  require(player_is_playing(), "long front-button repeat should restart the last Player job");

  sim::test::TempDirectory sleep_dir("carvera_sim_mainbutton_sleep_test");
  const auto& sleep_root = sleep_dir.path();
  write_config(sleep_root, "Sleep", 1);
  sim::SimulationInstance sleep_simulation(sim::test::persistent_sd_config(sleep_root));
  auto& sleep_runtime = sleep_simulation.firmware();
  auto& sleep_kernel = sleep_runtime.boot();

  press_front_button(sleep_runtime, 2);
  const auto sleep_panel = sleep_runtime.front_panel_state();
  require(sleep_kernel.is_sleeping(), "long front-button press should put firmware into Sleep state");
  require(sleep_kernel.is_halted(), "long front-button sleep should halt the firmware");
  require(!sleep_panel.power_rails.v12, "long front-button sleep should turn off the 12V rail");
  require(!sleep_panel.power_rails.v24, "long front-button sleep should turn off the 24V rail");
  return 0;
}
