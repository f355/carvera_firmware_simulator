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

function(sim_copy_firmware_source source_relpath output_name)
  configure_file(
    ${FIRMWARE_SRC}/${source_relpath}
    ${CMAKE_CURRENT_BINARY_DIR}/generated/${output_name}
    COPYONLY
  )
endfunction()

function(sim_glob_firmware_sources output_var)
  set(_sources)
  foreach(_pattern ${ARGN})
    file(GLOB _matches CONFIGURE_DEPENDS "${FIRMWARE_SRC}/${_pattern}")
    list(APPEND _sources ${_matches})
  endforeach()
  list(REMOVE_DUPLICATES _sources)
  set(${output_var} ${_sources} PARENT_SCOPE)
endfunction()

set(SIM_COPIED_FIRMWARE_SOURCES
  libs/Kernel.cpp:Kernel.cpp
  modules/robot/Block.cpp:Block.cpp
  modules/robot/Conveyor.cpp:Conveyor.cpp
  modules/robot/Planner.cpp:Planner.cpp
  modules/utils/player/Player.cpp:Player.cpp
  modules/tools/atc/ATCHandler.cpp:ATCHandler.cpp
  modules/tools/laser/Laser.cpp:Laser.cpp
  modules/utils/mainbutton/MainButton.cpp:MainButton.cpp
  modules/utils/simpleshell/SimpleShell.cpp:SimpleShell.cpp
  modules/utils/wifi/WifiProvider.cpp:WifiProvider.cpp
  modules/communication/SerialConsole2.cpp:SerialConsole2.cpp
)

foreach(_copy_spec ${SIM_COPIED_FIRMWARE_SOURCES})
  string(REPLACE ":" ";" _copy_parts "${_copy_spec}")
  list(GET _copy_parts 0 _copy_source)
  list(GET _copy_parts 1 _copy_output)
  sim_copy_firmware_source(${_copy_source} ${_copy_output})
endforeach()

set(SIM_FIRMWARE_FACADE_SOURCES
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
  src/core/interrupt_controller.cpp
  src/core/machine_geometry.cpp
  src/core/machine_state_snapshot.cpp
  src/core/main_button_led.cpp
  src/core/lpc1768_sim.cpp
  src/core/logging.cpp
  src/core/machine_simulator.cpp
  src/core/mbed_peripheral_state.cpp
  src/core/motion_pump.cpp
  src/core/motion_runner.cpp
  src/core/motion_telemetry.cpp
  src/core/physical_scene.cpp
  src/core/physical_signal_driver.cpp
  src/core/physical_tooling.cpp
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

set(SIM_GENERATED_FIRMWARE_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Kernel.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Block.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Conveyor.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Planner.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/MainButton.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/SimpleShell.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Player.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/Laser.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/ATCHandler.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/WifiProvider.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/generated/SerialConsole2.cpp
)

sim_glob_firmware_sources(FIRMWARE_LIB_SOURCES
  version.cpp
  libs/*.cpp
  libs/ConfigSources/FileConfigSource.cpp
)
list(FILTER FIRMWARE_LIB_SOURCES EXCLUDE REGEX "/libs/(Kernel|MRI_Hooks|FirmwareFileSystem|ahbmalloc|platform_memory|SDFAT|utils|Vector3|MemoryPool)\\.cpp$")

# Auto-detect source additions in the firmware subsystems the stock C1/CA1
# simulator actually links. Unsupported machine families and transports stay
# excluded here on purpose.
sim_glob_firmware_sources(FIRMWARE_MODULE_SOURCES
  modules/communication/*.cpp
  modules/communication/utils/*.cpp
  modules/utils/configurator/*.cpp
  modules/utils/player/*.[cC]*
  modules/robot/*.cpp
  modules/robot/arm_solutions/CartesianSolution.cpp
  modules/tools/switch/*.cpp
  modules/tools/endstops/*.cpp
  modules/tools/spindle/*.cpp
  modules/tools/zprobe/*.cpp
  modules/tools/temperaturecontrol/*.cpp
  modules/tools/temperatureswitch/*.cpp
  modules/tools/drillingcycles/*.cpp
)
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/communication/SerialConsole2\\.cpp$")
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/robot/(Block|Conveyor|Planner)\\.cpp$")
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/utils/player/Player\\.cpp$")
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/tools/spindle/(HuanyangSpindleControl|ModbusSpindleControl)\\.cpp$")
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/tools/zprobe/(DeltaCalibrationStrategy|DeltaGridStrategy|Plane3D|ThreePointStrategy)\\.cpp$")
list(FILTER FIRMWARE_MODULE_SOURCES EXCLUDE REGEX "/modules/tools/temperaturecontrol/PID_Autotuner\\.cpp$")
