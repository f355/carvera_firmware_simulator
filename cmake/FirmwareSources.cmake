# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

function(sim_config_bytes input_path output_var)
  file(READ "${input_path}" _config_hex HEX)
  string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," _config_bytes "${_config_hex}")
  set(${output_var} "${_config_bytes}" PARENT_SCOPE)
endfunction()

sim_config_bytes("${FIRMWARE_SRC}/config.default" SIM_CONFIG_DEFAULT_BYTES)
sim_config_bytes("${FIRMWARE_SRC}/config2.default" SIM_CONFIG2_DEFAULT_BYTES)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/firmware/firm_config_data.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/firm_config_data.cpp
  @ONLY
)

set(SIM_FIRMWARE_FACADE_SOURCES
  src/compat/active_context.cpp
  src/firmware/firmware_boot_stubs.cpp
  src/firmware/firm_config_source.cpp
  src/firmware/host_filesystem.cpp
  src/firmware/i2c_sim.cpp
  src/firmware/main_button_led_stub.cpp
  src/firmware/mri_hooks_stub.cpp
  src/firmware/utils_stubs.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/firm_config_data.cpp
)

set(SIM_CORE_SOURCES
  src/core/adc_sim.cpp
  src/core/atc_rack_model.cpp
  src/core/board_profile.cpp
  src/core/delay_hooks.cpp
  src/core/event_engine.cpp
  src/core/interrupt_controller.cpp
  src/core/machine_geometry.cpp
  src/core/machine_state_snapshot.cpp
  src/core/main_button_led.cpp
  src/core/lpc1768_sim.cpp
  src/core/logging.cpp
  src/core/machine_simulator.cpp
  src/core/mbed_peripheral_state.cpp
  src/core/motion_telemetry.cpp
  src/core/physical_scene.cpp
  src/core/physical_signal_driver.cpp
  src/core/physical_tooling.cpp
  src/core/persistent_machine_state.cpp
  src/core/probe_contact_model.cpp
  src/core/realtime_timer_pacer.cpp
  src/core/robot_axis_binding.cpp
  src/core/simulator_context.cpp
  src/core/spindle_state.cpp
  src/core/stepper_axis.cpp
  src/core/timer_irq.cpp
  src/core/timer_scheduler.cpp
  src/core/us_ticker_sim.cpp
  src/core/virtual_clock.cpp
  src/core/watchdog.cpp
)

set(SIM_PROTOCOL_SOURCES
  src/protocol/m8266_wifi.cpp
)

set(SIM_RUNTIME_SUPPORT_SOURCES
  src/runtime/runtime_atc_config.cpp
  src/runtime/runtime_motor_alarm_wiring.cpp
  src/runtime/runtime_modules.cpp
  src/runtime/runtime_pin_config.cpp
  src/runtime/runtime_temperature.cpp
)

# This manifest is intentionally explicit. The simulator is pinned to one
# compatible firmware commit, so additions to the supported firmware surface
# should be reviewed instead of silently entering the host build through a glob.
set(CARVERA_FIRMWARE_SOURCES
  ${FIRMWARE_SRC}/version.cpp
  ${FIRMWARE_SRC}/libs/Adc.cpp
  ${FIRMWARE_SRC}/libs/AppendFileStream.cpp
  ${FIRMWARE_SRC}/libs/Config.cpp
  ${FIRMWARE_SRC}/libs/ConfigCache.cpp
  ${FIRMWARE_SRC}/libs/ConfigSource.cpp
  ${FIRMWARE_SRC}/libs/ConfigValue.cpp
  ${FIRMWARE_SRC}/libs/Hook.cpp
  ${FIRMWARE_SRC}/libs/Kernel.cpp
  ${FIRMWARE_SRC}/libs/Module.cpp
  ${FIRMWARE_SRC}/libs/Pin.cpp
  ${FIRMWARE_SRC}/libs/PublicData.cpp
  ${FIRMWARE_SRC}/libs/Pwm.cpp
  ${FIRMWARE_SRC}/libs/SlowTicker.cpp
  ${FIRMWARE_SRC}/libs/SoftPWM.cpp
  ${FIRMWARE_SRC}/libs/StepTicker.cpp
  ${FIRMWARE_SRC}/libs/StepperMotor.cpp
  ${FIRMWARE_SRC}/libs/StreamOutput.cpp
  ${FIRMWARE_SRC}/libs/Watchdog.cpp
  ${FIRMWARE_SRC}/libs/gpio.cpp
  ${FIRMWARE_SRC}/libs/md5.cpp
  ${FIRMWARE_SRC}/libs/ConfigSources/FileConfigSource.cpp
  ${FIRMWARE_SRC}/modules/communication/GcodeDispatch.cpp
  ${FIRMWARE_SRC}/modules/communication/SerialConsole.cpp
  ${FIRMWARE_SRC}/modules/communication/SerialConsole2.cpp
  ${FIRMWARE_SRC}/modules/communication/utils/Gcode.cpp
  ${FIRMWARE_SRC}/modules/utils/configurator/Configurator.cpp
  ${FIRMWARE_SRC}/modules/utils/mainbutton/MainButton.cpp
  ${FIRMWARE_SRC}/modules/utils/player/OCodeHandler.cpp
  ${FIRMWARE_SRC}/modules/utils/player/Player.cpp
  ${FIRMWARE_SRC}/modules/utils/player/quicklz.c
  ${FIRMWARE_SRC}/modules/utils/simpleshell/SimpleShell.cpp
  ${FIRMWARE_SRC}/modules/utils/wifi/WifiProvider.cpp
  ${FIRMWARE_SRC}/modules/robot/Block.cpp
  ${FIRMWARE_SRC}/modules/robot/BlockQueue.cpp
  ${FIRMWARE_SRC}/modules/robot/Conveyor.cpp
  ${FIRMWARE_SRC}/modules/robot/Planner.cpp
  ${FIRMWARE_SRC}/modules/robot/Robot.cpp
  ${FIRMWARE_SRC}/modules/robot/arm_solutions/CartesianSolution.cpp
  ${FIRMWARE_SRC}/modules/tools/atc/ATCHandler.cpp
  ${FIRMWARE_SRC}/modules/tools/endstops/Endstops.cpp
  ${FIRMWARE_SRC}/modules/tools/laser/Laser.cpp
  ${FIRMWARE_SRC}/modules/tools/spindle/AnalogSpindleControl.cpp
  ${FIRMWARE_SRC}/modules/tools/spindle/PIDPWMSpindleControl.cpp
  ${FIRMWARE_SRC}/modules/tools/spindle/PWMSpindleControl.cpp
  ${FIRMWARE_SRC}/modules/tools/spindle/SpindleControl.cpp
  ${FIRMWARE_SRC}/modules/tools/spindle/SpindleMaker.cpp
  ${FIRMWARE_SRC}/modules/tools/switch/Switch.cpp
  ${FIRMWARE_SRC}/modules/tools/switch/SwitchPool.cpp
  ${FIRMWARE_SRC}/modules/tools/zprobe/CartGridStrategy.cpp
  ${FIRMWARE_SRC}/modules/tools/zprobe/ZProbe.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/AD8495.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/PT100_E3D.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/TemperatureControl.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/TemperatureControlPool.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/Thermistor.cpp
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol/max31855.cpp
  ${FIRMWARE_SRC}/modules/tools/temperatureswitch/TemperatureSwitch.cpp
  ${FIRMWARE_SRC}/modules/tools/drillingcycles/Drillingcycles.cpp
)

foreach(_firmware_source IN LISTS CARVERA_FIRMWARE_SOURCES)
  if(NOT EXISTS "${_firmware_source}")
    message(FATAL_ERROR "Pinned firmware source is missing: ${_firmware_source}")
  endif()
endforeach()
