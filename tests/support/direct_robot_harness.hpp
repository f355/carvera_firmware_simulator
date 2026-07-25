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

#ifndef CARVERA_SIMULATOR_TESTS_SUPPORT_DIRECT_ROBOT_HARNESS_HPP
#define CARVERA_SIMULATOR_TESTS_SUPPORT_DIRECT_ROBOT_HARNESS_HPP

#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "assertions.hpp"
#include "memory_config.hpp"

#include "Config.h"
#include "Conveyor.h"
#include "Robot.h"
#include "StepTicker.h"
#include "libs/Kernel.h"
#include "sim/event_engine.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/persistent_machine_state.hpp"
#include "direct_robot_config.hpp"

namespace sim::test {

class ConfiguredPersistentMachineState : public PersistentMachineState {
 public:
  ConfiguredPersistentMachineState(MachineModel model, std::uint8_t function_setting) {
    eeprom().reset();
    eeprom().configure_factory_settings({model, function_setting});
  }
};

class DirectRobotHarness {
 public:
  explicit DirectRobotHarness(std::vector<std::string> extra_config = {}, MachineModel model = MachineModel::CarveraC1,
                              std::uint8_t function_setting = 0x04)
      : persistent(model, function_setting), simulator(persistent), event_engine(simulator) {
    axis_ids = {
        simulator.add_step_dir_axis({1, 18}, {1, 20}), simulator.add_step_dir_axis({1, 19}, {1, 21}),
        simulator.add_step_dir_axis({1, 22}, {1, 23}), simulator.add_step_dir_axis({1, 24}, {1, 25}),
        simulator.add_step_dir_axis({1, 26}, {1, 27}),
    };

    auto config_lines = direct_robot_config_lines();
    config_lines.insert(config_lines.end(), std::make_move_iterator(extra_config.begin()),
                        std::make_move_iterator(extra_config.end()));
    kernel.config = new Config(new MemoryConfigSource(std::move(config_lines)));
    kernel.config->config_cache_load();
  }

  void load_robot() {
    kernel.robot->on_module_loaded();
    kernel.conveyor->start(kernel.robot->get_number_registered_motors());
    kernel.step_ticker->start();
  }

  EventRunResult run_until_idle(std::size_t max_step_ticks = 200'000) {
    return event_engine.run_until_motion_idle(kernel, max_step_ticks);
  }

  ConfiguredPersistentMachineState persistent;
  MachineSimulator simulator;
  Kernel kernel;
  EventEngine event_engine;
  std::array<std::size_t, 5> axis_ids{};
};

}  // namespace sim::test

#endif
