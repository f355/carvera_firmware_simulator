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

# Player stores std::string search results in a 32-bit integer. That happens to
# match size_type on the MCU, but truncates npos on 64-bit simulator hosts and
# makes every uncompressed upload look like an .lz upload. Compile a corrected
# build-tree copy while the pinned firmware still contains that assumption.
set(_player_source "${FIRMWARE_SRC}/modules/utils/player/Player.cpp")
set(_player_host_source "${CMAKE_CURRENT_BINARY_DIR}/generated/Player.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_player_source}")
file(READ "${_player_source}" _player_contents)
set(_player_npos_32bit "unsigned int start_pos = filename.find(\".lz\");")
set(_player_npos_native "string::size_type start_pos = filename.find(\".lz\");")
string(REPLACE "${_player_npos_32bit}" "${_player_npos_native}" _player_host_contents "${_player_contents}")
if(_player_host_contents STREQUAL _player_contents)
  message(FATAL_ERROR "Pinned Player.cpp no longer contains the expected 32-bit upload position")
endif()
file(WRITE "${_player_host_source}" "${_player_host_contents}")

# The firmware uses strdup for command/file-name buffers. Route those few C
# allocations through the same LPC heap model as C++ allocations.
set(_gcode_source "${FIRMWARE_SRC}/modules/communication/utils/Gcode.cpp")
set(_gcode_host_source "${CMAKE_CURRENT_BINARY_DIR}/generated/Gcode.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_gcode_source}")
file(READ "${_gcode_source}" _gcode_contents)
string(REPLACE "strdup(" "sim::lpc_memory::tracked_strdup(" _gcode_host_contents "${_gcode_contents}")
string(REPLACE "free(" "sim::lpc_memory::tracked_free(" _gcode_host_contents "${_gcode_host_contents}")
if(_gcode_host_contents STREQUAL _gcode_contents)
  message(FATAL_ERROR "Pinned Gcode.cpp no longer contains the expected strdup/free calls")
endif()
string(PREPEND _gcode_host_contents "#include \"sim/lpc_memory_accounting.hpp\"\n")
file(WRITE "${_gcode_host_source}" "${_gcode_host_contents}")

set(_firmware_override_include "${CMAKE_CURRENT_BINARY_DIR}/generated/firmware_overrides")
file(MAKE_DIRECTORY "${_firmware_override_include}")
set(_append_file_stream_source "${FIRMWARE_SRC}/libs/AppendFileStream.h")
set(_append_file_stream_implementation "${FIRMWARE_SRC}/libs/AppendFileStream.cpp")
set(_append_file_stream_host_header "${_firmware_override_include}/AppendFileStream.h")
set(_append_file_stream_host_implementation "${CMAKE_CURRENT_BINARY_DIR}/generated/AppendFileStream.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${_append_file_stream_source}"
  "${_append_file_stream_implementation}"
)
file(READ "${_append_file_stream_source}" _append_file_stream_contents)
string(REPLACE "strdup(filename)" "sim::lpc_memory::tracked_strdup(filename)"
  _append_file_stream_host_contents "${_append_file_stream_contents}")
string(REPLACE "free(fn)" "sim::lpc_memory::tracked_free(fn)"
  _append_file_stream_host_contents "${_append_file_stream_host_contents}")
if(_append_file_stream_host_contents STREQUAL _append_file_stream_contents)
  message(FATAL_ERROR "Pinned AppendFileStream.h no longer contains the expected strdup/free calls")
endif()
string(REPLACE "#include \"StreamOutput.h\""
  "#include \"StreamOutput.h\"\n#include \"sim/lpc_memory_accounting.hpp\""
  _append_file_stream_host_contents "${_append_file_stream_host_contents}")
file(WRITE "${_append_file_stream_host_header}" "${_append_file_stream_host_contents}")
file(READ "${_append_file_stream_implementation}" _append_file_stream_implementation_contents)
file(WRITE "${_append_file_stream_host_implementation}" "${_append_file_stream_implementation_contents}")

# include/FATFileSystem.h stubs out ChaNFS for the host but must keep the
# firmware's path budget, which SimpleShell's file-integrity commands enforce.
set(_fatfs_header "${FIRMWARE_SRC}/libs/ChaNFS/FATFileSystem.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_fatfs_header}")
file(READ "${_fatfs_header}" _fatfs_header_contents)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/FATFileSystem.h" _fatfs_stub_contents)
string(REGEX MATCH "define[ \t]+FATFS_PATH_MAX[ \t]+([0-9]+)" _fatfs_match "${_fatfs_header_contents}")
set(_fatfs_firmware_limit "${CMAKE_MATCH_1}")
string(REGEX MATCH "define[ \t]+FATFS_PATH_MAX[ \t]+([0-9]+)" _fatfs_match "${_fatfs_stub_contents}")
set(_fatfs_stub_limit "${CMAKE_MATCH_1}")
if(NOT _fatfs_firmware_limit OR NOT _fatfs_stub_limit)
  message(FATAL_ERROR "Could not read FATFS_PATH_MAX from the pinned firmware or the simulator stub")
endif()
if(NOT _fatfs_firmware_limit STREQUAL _fatfs_stub_limit)
  message(FATAL_ERROR
    "include/FATFileSystem.h defines FATFS_PATH_MAX ${_fatfs_stub_limit}, but the pinned firmware uses "
    "${_fatfs_firmware_limit}. Update the simulator stub to match."
  )
endif()

# WifiProvider pins a buffer into LPC AHBSRAM with an ELF-only section name.
# Mach-O rejects that form, so compile a host copy that drops the placement.
set(_wifi_source "${FIRMWARE_SRC}/modules/utils/wifi/WifiProvider.cpp")
set(_wifi_host_source "${CMAKE_CURRENT_BINARY_DIR}/generated/WifiProvider.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_wifi_source}")
file(READ "${_wifi_source}" _wifi_contents)
set(_wifi_ahbsram_attr "__attribute__((section(\"AHBSRAM1\"), aligned(4)))")
set(_wifi_host_attr "alignas(4)")
string(REPLACE "${_wifi_ahbsram_attr}" "${_wifi_host_attr}" _wifi_host_contents "${_wifi_contents}")
if(_wifi_host_contents STREQUAL _wifi_contents)
  message(FATAL_ERROR "Pinned WifiProvider.cpp no longer contains the expected AHBSRAM1 section attribute")
endif()
string(REPLACE "strdup(" "sim::lpc_memory::tracked_strdup(" _wifi_host_contents "${_wifi_host_contents}")
string(REPLACE "free(" "sim::lpc_memory::tracked_free(" _wifi_host_contents "${_wifi_host_contents}")
string(PREPEND _wifi_host_contents "#include \"sim/lpc_memory_accounting.hpp\"\n")
file(WRITE "${_wifi_host_source}" "${_wifi_host_contents}")

# The target SimpleShell walks newlib-nano's in-memory chunk headers. Host
# allocators have unrelated layouts, so route only the mem command through the
# simulator's LPC shadow-accounting report.
set(_simpleshell_source "${FIRMWARE_SRC}/modules/utils/simpleshell/SimpleShell.cpp")
set(_simpleshell_host_source "${CMAKE_CURRENT_BINARY_DIR}/generated/SimpleShell.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_simpleshell_source}")
file(READ "${_simpleshell_source}" _simpleshell_contents)
set(_simpleshell_target_mem [=[void SimpleShell::mem_command( string parameters, StreamOutput *stream)
{
    bool verbose = shift_parameter( parameters ).find_first_of("Vv") != string::npos;
    unsigned long heap_top = (unsigned long)_sbrk(0);
    unsigned long heap_unallocated_top = (STACK_SIZE && g_maximumHeapAddress != 0) ? g_maximumHeapAddress - heap_top : 0; // Calculate unallocated space at the top if stack limit is set
    stream->printf("Main Heap Unallocated Top: %lu bytes\r\n", heap_unallocated_top);

    uint32_t heap_fragmented_free = heapWalk(stream, verbose); // Calculates and prints used/free within allocated heap part
    stream->printf("Total Free RAM (Main Heap): %lu bytes\r\n", heap_unallocated_top + heap_fragmented_free);

    // Use MemoryPool::free() which calculates total free space in the pool
    uint32_t ahb_total_free = AHB.free();
    stream->printf("AHB Pool Total Free: %lu bytes\r\n", ahb_total_free);

    if (verbose) {
        stream->printf("--- AHB Pool Details ---\n");
        AHB.debug(stream); // Detailed AHB pool breakdown
        stream->printf("--- End AHB Pool Details ---\n");
    }

    stream->printf("Block size: %u bytes, Tickinfo size: %u bytes\n", sizeof(Block), sizeof(Block::tickinfo_t) * Block::n_actuators);
}]=])
set(_simpleshell_host_mem [=[void SimpleShell::mem_command( string parameters, StreamOutput *stream)
{
    bool verbose = shift_parameter( parameters ).find_first_of("Vv") != string::npos;
    sim::lpc_memory::print_memory_report(stream, verbose);
}]=])
string(REPLACE "${_simpleshell_target_mem}" "${_simpleshell_host_mem}"
  _simpleshell_host_contents "${_simpleshell_contents}")
if(_simpleshell_host_contents STREQUAL _simpleshell_contents)
  message(FATAL_ERROR "Pinned SimpleShell.cpp no longer contains the expected target mem command")
endif()
file(WRITE "${_simpleshell_host_source}" "${_simpleshell_host_contents}")

set(SIM_FIRMWARE_FACADE_SOURCES
  src/compat/active_context.cpp
  src/firmware/firmware_boot_stubs.cpp
  src/firmware/firm_config_source.cpp
  src/firmware/firmware_allocation_hooks.cpp
  src/firmware/host_filesystem.cpp
  src/firmware/i2c_sim.cpp
  src/firmware/lpc_allocation_resolver.cpp
  src/firmware/lpc_function_symbols.cpp
  src/firmware/lpc_memory_accounting.cpp
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
  src/protocol/makera_protocol.cpp
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
  ${_append_file_stream_host_implementation}
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
  ${_gcode_host_source}
  ${FIRMWARE_SRC}/modules/utils/configurator/Configurator.cpp
  ${FIRMWARE_SRC}/modules/utils/mainbutton/MainButton.cpp
  ${FIRMWARE_SRC}/modules/utils/player/OCodeHandler.cpp
  ${_player_host_source}
  ${FIRMWARE_SRC}/modules/utils/player/quicklz.c
  ${_simpleshell_host_source}
  ${_wifi_host_source}
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
