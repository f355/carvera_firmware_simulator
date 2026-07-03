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

#include <cmath>

#include "test_support.hpp"

#define private public
#include "Robot.h"
#undef private

#include "Config.h"
#include "Conveyor.h"
#include "LPC17xx.h"
#include "StepTicker.h"
#include "StepperMotor.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "sim/event_engine.hpp"
#include "sim/robot_axis_binding.hpp"
#include "sim/stepper_axis.hpp"

namespace {

using sim::test::MemoryConfigSource;
using sim::test::require;

bool near(float lhs, float rhs) { return std::fabs(lhs - rhs) < 0.001F; }

void pulse_pin(LPC_GPIO_TypeDef* port, std::uint8_t pin) {
  port->FIOSET = 1u << pin;
  port->FIOCLR = 1u << pin;
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  kernel.config = new Config(new MemoryConfigSource({
      "arm_solution cartesian\n", "alpha_step_pin 1.28\n",
      "alpha_dir_pin 1.29\n",     "alpha_en_pin nc\n",
      "alpha_steps_per_mm 200\n", "alpha_max_rate 3000\n",
      "beta_step_pin 1.26\n",     "beta_dir_pin 1.27\n",
      "beta_en_pin nc\n",         "beta_steps_per_mm 200\n",
      "beta_max_rate 3000\n",     "gamma_step_pin 1.24\n",
      "gamma_dir_pin 1.25\n",     "gamma_en_pin nc\n",
      "gamma_steps_per_mm 200\n", "gamma_max_rate 2000\n",
      "delta_step_pin 1.18\n",    "delta_dir_pin 1.20!\n",
      "delta_en_pin 3.26\n",      "delta_steps_per_mm 26.666667\n",
      "delta_max_rate 1800\n",    "delta_acceleration 360\n",
      "epsilon_step_pin 1.21\n",  "epsilon_dir_pin 1.23\n",
      "epsilon_en_pin 1.30\n",    "epsilon_steps_per_mm 43200\n",
      "epsilon_max_rate 100\n",   "epsilon_acceleration 10\n",
      "acceleration 150\n",       "soft_endstop.enable false\n",
  }));
  kernel.config->config_cache_load();
  kernel.factory_set->MachineModel = CARVERA_AIR;
  kernel.factory_set->FuncSetting |= 0x01;
  kernel.robot->load_config();

  require(kernel.robot->get_number_registered_motors() == 5, "Robot should register five contiguous stock motors");
  require(kernel.robot->actuators[3]->get_motor_id() == 3, "delta motor should be registered at index 3");
  require(near(kernel.robot->actuators[3]->get_steps_per_mm(), 26.666667F), "delta steps/mm should come from config");
  require(near(kernel.robot->actuators[3]->get_max_rate(), 30.0F),
          "delta max rate should be converted from mm/min to mm/s");
  require(near(kernel.robot->actuators[3]->get_acceleration(), 360.0F), "delta acceleration should come from config");

  sim::attach_configured_stepper_axes(kernel);
  require(std::fabs(simulator.axis_position_mm(3) - 10.0) < 0.05,
          "physical A axis should boot off-index so homing visibly moves it");
  const auto physical_delta_initial_steps = simulator.axis_position_steps(3);
  kernel.conveyor->start(kernel.robot->get_number_registered_motors());
  kernel.step_ticker->start();

  const float delta[4] = {0.0F, 0.0F, 0.0F, 1.0F};
  require(kernel.robot->delta_move(delta, 10.0F, 4), "Robot should queue a config-created delta-axis jog");

  sim::EventEngine engine(simulator);
  require(engine.run_until_motion_idle(kernel, 100'000).status == sim::EventRunStatus::ConditionReached,
          "simulator should execute config-created Robot motion to idle");
  require(
      simulator.axis_position_steps(3) - physical_delta_initial_steps == kernel.robot->actuators[3]->get_current_step(),
      "configured inverted direction pins should make physical steps match the firmware StepperMotor");

  simulator.reset();

  Kernel shared_switch_kernel;
  shared_switch_kernel.config = new Config(new MemoryConfigSource({
      "arm_solution cartesian\n",
      "alpha_step_pin 1.18\n",
      "alpha_dir_pin 1.20\n",
      "alpha_en_pin nc\n",
      "alpha_steps_per_mm 10\n",
      "alpha_max_rate 3000\n",
      "alpha_min_endstop 0.24^\n",
      "alpha_max_endstop 0.24^\n",
      "alpha_homing_direction home_to_max\n",
      "alpha_max_travel 20\n",
      "acceleration 150\n",
      "soft_endstop.enable false\n",
  }));
  shared_switch_kernel.config->config_cache_load();
  shared_switch_kernel.robot->load_config();
  sim::attach_configured_stepper_axes(shared_switch_kernel);

  require(sim::stepper_axes::count() == 1, "shared-switch test should attach one configured physical axis");
  require(!simulator.axis_endstop_triggered(0, sim::EndstopSide::Min),
          "shared min side should start released at the boot position");
  require(!simulator.axis_endstop_triggered(0, sim::EndstopSide::Max),
          "shared max side should start released at the boot position");
  require(!simulator.gpio_level({0, 24}), "shared min/max GPIO should start inactive inside travel");

  LPC_GPIO1->FIOCLR = 1u << 20;
  for (int i = 0; i < 130; ++i) {
    pulse_pin(LPC_GPIO1, 18);
  }
  require(simulator.axis_endstop_triggered(0, sim::EndstopSide::Max),
          "configured shared max side should trigger at positive travel");
  require(!simulator.axis_endstop_triggered(0, sim::EndstopSide::Min),
          "configured shared min side should stay released at positive travel");
  require(simulator.gpio_level({0, 24}), "shared min/max GPIO should assert at the max side");

  LPC_GPIO1->FIOSET = 1u << 20;
  for (int i = 0; i < 260; ++i) {
    pulse_pin(LPC_GPIO1, 18);
  }
  require(simulator.axis_endstop_triggered(0, sim::EndstopSide::Min),
          "configured shared min side should trigger at negative travel");
  require(!simulator.axis_endstop_triggered(0, sim::EndstopSide::Max),
          "configured shared max side should release at negative travel");
  require(simulator.gpio_level({0, 24}), "shared min/max GPIO should assert at the min side");

  simulator.reset();

  Kernel ca1_shared_switch_kernel;
  ca1_shared_switch_kernel.config = new Config(new MemoryConfigSource({
      "arm_solution cartesian\n",
      "alpha_step_pin 1.18\n",
      "alpha_dir_pin 1.20\n",
      "alpha_en_pin nc\n",
      "alpha_steps_per_mm 10\n",
      "alpha_max_rate 3000\n",
      "alpha_min_endstop 0.24^\n",
      "alpha_max_endstop 0.24^\n",
      "alpha_homing_direction home_to_max\n",
      "alpha_max_travel 20\n",
      "acceleration 150\n",
      "soft_endstop.enable false\n",
  }));
  ca1_shared_switch_kernel.config->config_cache_load();
  ca1_shared_switch_kernel.robot->load_config();
  sim::attach_configured_stepper_axes(ca1_shared_switch_kernel, sim::MachineModel::CarveraAirCA1);

  LPC_GPIO1->FIOSET = 1u << 20;
  for (int i = 0; i < 260; ++i) {
    pulse_pin(LPC_GPIO1, 18);
  }
  require(!simulator.axis_endstop_triggered(0, sim::EndstopSide::Min),
          "CA1 should not simulate absent negative cartesian limit switches");
  require(!simulator.gpio_level({0, 24}), "CA1 shared homing GPIO should stay inactive past negative travel");

  LPC_GPIO1->FIOCLR = 1u << 20;
  for (int i = 0; i < 520; ++i) {
    pulse_pin(LPC_GPIO1, 18);
  }
  require(simulator.axis_endstop_triggered(0, sim::EndstopSide::Max),
          "CA1 positive cartesian switch should still trip at the real homing side");
  require(simulator.gpio_level({0, 24}), "CA1 shared homing GPIO should assert at positive travel");

  return 0;
}
