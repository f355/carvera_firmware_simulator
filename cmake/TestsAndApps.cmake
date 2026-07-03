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

enable_testing()
include(ProcessorCount)

set(SIM_DEFAULT_TEST_TIMEOUT_SECONDS 30 CACHE STRING "Default timeout for simulator CTest tests")
set(SIM_LONG_TEST_TIMEOUT_SECONDS 90 CACHE STRING "Timeout for simulator integration CTest tests")
set(SIM_EXTRA_LONG_TEST_TIMEOUT_SECONDS 180 CACHE STRING "Timeout for the heaviest simulator integration CTest tests")

function(sim_register_test name)
  set(options)
  set(one_value_args TIMEOUT)
  set(multi_value_args LABELS)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  if(NOT ARG_TIMEOUT)
    set(ARG_TIMEOUT ${SIM_DEFAULT_TEST_TIMEOUT_SECONDS})
  endif()
  if(NOT ARG_LABELS)
    set(ARG_LABELS fast)
  endif()
  set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT})
  set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}")
  set_property(GLOBAL APPEND PROPERTY SIM_TEST_TARGETS ${name})
endfunction()

function(sim_add_test name link_target)
  set(options)
  set(one_value_args TIMEOUT)
  set(multi_value_args LABELS)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
  add_executable(${name} tests/${name}.cpp)
  sim_force_include(${name} ${HOST_PRELUDE})
  target_link_libraries(${name} PRIVATE ${link_target})
  sim_enable_test_diagnostics(${name})
  add_test(NAME ${name} COMMAND ${name})
  set(register_args)
  if(ARG_TIMEOUT)
    list(APPEND register_args TIMEOUT ${ARG_TIMEOUT})
  endif()
  if(ARG_LABELS)
    list(APPEND register_args LABELS ${ARG_LABELS})
  endif()
  sim_register_test(${name} ${register_args})
endfunction()

function(sim_add_runtime_test name)
  sim_add_test(${name} carvera_sim_api TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)
endfunction()

set(SIM_RUNTIME_TESTS
  api_conversions_test
  api_eeprom_test
  api_jog_test
  api_physical_io_test
  api_safety_front_panel_test
  api_serial_test
  api_service_test
  api_snapshot_tooling_test
  api_temperature_switch_test
  atc_interactive_transport_test
  atc_m6_runtime_test
  atc_persistent_tool_boot_test
  ca1_manual_toolchange_test
  controller_handshake_test
  factory_settings_mount_order_test
  free_running_homing_test
  g38_3d_probe_side_test
  g38_stock_probe_test
  g38_tool_setter_probe_test
  hard_limit_recovery_test
  interactive_io_test
  machine_state_snapshot_test
  mainbutton_runtime_test
  motor_alarm_api_test
  nonblocking_snapshot_homed_test
  player_laser_runtime_test
  realtime_motion_speed_test
  rotary_axis_runtime_test
  runtime_boot_composition_test
  runtime_temperature_reporting_test
  safety_inputs_test
  safety_lifecycle_test
  simulation_instance_test
  simpleshell_configurator_test
  stock_z_probe_side_crash_test
  switch_api_test
  temperature_fault_runtime_test
  wifi_protocol_test
  zigbee_probe_protocol_test
  zprobe_crash_detection_test
)

set(SIM_SPECIAL_TESTS
  firmware_boot_test
  stdio_api_test
  stream_busy_control_test
  stream_interactive_transport_test
  stream_player_realtime_speed_test
  stream_startup_telemetry_test
)

file(GLOB SIM_ALL_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.cpp)
set(SIM_FIRMWARE_SIM_TESTS)
foreach(test_source IN LISTS SIM_ALL_TEST_SOURCES)
  get_filename_component(test_name ${test_source} NAME_WE)
  if(test_name IN_LIST SIM_RUNTIME_TESTS OR test_name IN_LIST SIM_SPECIAL_TESTS)
    continue()
  endif()
  list(APPEND SIM_FIRMWARE_SIM_TESTS ${test_name})
endforeach()
list(SORT SIM_FIRMWARE_SIM_TESTS)

foreach(test_name IN LISTS SIM_FIRMWARE_SIM_TESTS)
  sim_add_test(${test_name} carvera_firmware_sim)
endforeach()

foreach(test_name IN LISTS SIM_RUNTIME_TESTS)
  sim_add_runtime_test(${test_name})
endforeach()
set_tests_properties(atc_m6_runtime_test PROPERTIES TIMEOUT ${SIM_EXTRA_LONG_TEST_TIMEOUT_SECONDS})
set_tests_properties(realtime_timer_pacing_test realtime_motion_speed_test PROPERTIES RUN_SERIAL TRUE)

target_compile_definitions(free_running_homing_test PRIVATE CARVERA_FIRMWARE_ROOT="${CARVERA_FIRMWARE_ROOT}")

add_executable(carvera_sim_stdio src/apps/sim_stdio_server.cpp)
sim_force_include(carvera_sim_stdio ${HOST_PRELUDE})
target_link_libraries(carvera_sim_stdio PRIVATE carvera_sim_api)
sim_enable_project_diagnostics(carvera_sim_stdio)

add_executable(carvera_sim_stream_stdio src/apps/sim_stream_stdio_server.cpp)
sim_force_include(carvera_sim_stream_stdio ${HOST_PRELUDE})
target_link_libraries(carvera_sim_stream_stdio PRIVATE carvera_sim_api)
sim_enable_project_diagnostics(carvera_sim_stream_stdio)

add_executable(carvera_sim_interactive src/apps/sim_interactive.cpp)
sim_force_include(carvera_sim_interactive ${HOST_PRELUDE})
target_link_libraries(carvera_sim_interactive PRIVATE carvera_sim_api)
sim_enable_project_diagnostics(carvera_sim_interactive)

add_executable(stdio_api_test tests/stdio_api_test.cpp)
sim_force_include(stdio_api_test ${HOST_PRELUDE})
target_link_libraries(stdio_api_test PRIVATE carvera_sim_api)
sim_enable_test_diagnostics(stdio_api_test)
add_dependencies(stdio_api_test carvera_sim_stdio)
add_test(NAME stdio_api_test COMMAND stdio_api_test $<TARGET_FILE:carvera_sim_stdio>)
sim_register_test(stdio_api_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)

add_executable(stream_interactive_transport_test tests/stream_interactive_transport_test.cpp)
sim_force_include(stream_interactive_transport_test ${HOST_PRELUDE})
target_link_libraries(stream_interactive_transport_test PRIVATE carvera_sim_api)
sim_enable_test_diagnostics(stream_interactive_transport_test)
add_dependencies(stream_interactive_transport_test carvera_sim_stream_stdio)
add_test(NAME stream_interactive_transport_test
  COMMAND stream_interactive_transport_test $<TARGET_FILE:carvera_sim_stream_stdio>)
sim_register_test(stream_interactive_transport_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)

add_executable(stream_startup_telemetry_test tests/stream_startup_telemetry_test.cpp)
sim_force_include(stream_startup_telemetry_test ${HOST_PRELUDE})
target_link_libraries(stream_startup_telemetry_test PRIVATE carvera_sim_api)
sim_enable_test_diagnostics(stream_startup_telemetry_test)
add_dependencies(stream_startup_telemetry_test carvera_sim_stream_stdio)
add_test(NAME stream_startup_telemetry_test
  COMMAND stream_startup_telemetry_test $<TARGET_FILE:carvera_sim_stream_stdio>)
sim_register_test(stream_startup_telemetry_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)

add_executable(stream_busy_control_test tests/stream_busy_control_test.cpp)
sim_force_include(stream_busy_control_test ${HOST_PRELUDE})
target_link_libraries(stream_busy_control_test PRIVATE carvera_sim_api)
sim_enable_test_diagnostics(stream_busy_control_test)
add_dependencies(stream_busy_control_test carvera_sim_stream_stdio)
add_test(NAME stream_busy_control_test
  COMMAND stream_busy_control_test $<TARGET_FILE:carvera_sim_stream_stdio>)
sim_register_test(stream_busy_control_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)

add_executable(stream_player_realtime_speed_test tests/stream_player_realtime_speed_test.cpp)
sim_force_include(stream_player_realtime_speed_test ${HOST_PRELUDE})
target_link_libraries(stream_player_realtime_speed_test PRIVATE carvera_sim_api)
sim_enable_test_diagnostics(stream_player_realtime_speed_test)
add_dependencies(stream_player_realtime_speed_test carvera_sim_stream_stdio)
add_test(NAME stream_player_realtime_speed_test
  COMMAND stream_player_realtime_speed_test $<TARGET_FILE:carvera_sim_stream_stdio>)
sim_register_test(stream_player_realtime_speed_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)
set_tests_properties(stream_player_realtime_speed_test PROPERTIES RUN_SERIAL TRUE)
add_library(carvera_firmware_boot_main OBJECT tests/support/firmware_main_entry.cpp)
target_include_directories(carvera_firmware_boot_main PRIVATE include)
target_include_directories(carvera_firmware_boot_main SYSTEM PRIVATE ${CARVERA_FIRMWARE_INCLUDE_DIRS})
target_compile_definitions(carvera_firmware_boot_main PRIVATE
  CHECKSUM_USE_CPP
  CARVERA_FIRMWARE_MAIN="${FIRMWARE_SRC}/main.cpp"
  main=firmware_main
  NO_TOOLS_SCARACAL=1
  NO_TOOLS_ROTARYDELTACALIBRATION=1
)
sim_force_include(carvera_firmware_boot_main ${HOST_PRELUDE})
sim_enable_sanitizers(carvera_firmware_boot_main)

add_executable(firmware_boot_test
  tests/firmware_boot_test.cpp
  $<TARGET_OBJECTS:carvera_firmware_boot_main>
)
sim_force_include(firmware_boot_test ${HOST_PRELUDE})
target_link_libraries(firmware_boot_test PRIVATE carvera_firmware_sim)
sim_enable_test_diagnostics(firmware_boot_test)
add_test(NAME firmware_boot_test COMMAND firmware_boot_test)
sim_register_test(firmware_boot_test TIMEOUT ${SIM_LONG_TEST_TIMEOUT_SECONDS} LABELS integration)

ProcessorCount(SIM_TEST_JOBS)
if(SIM_TEST_JOBS EQUAL 0)
  set(SIM_TEST_JOBS 4)
elseif(SIM_TEST_JOBS GREATER 8)
  set(SIM_TEST_JOBS 8)
endif()

get_property(SIM_TEST_TARGETS GLOBAL PROPERTY SIM_TEST_TARGETS)
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure -j ${SIM_TEST_JOBS}
  DEPENDS ${SIM_TEST_TARGETS}
  USES_TERMINAL
)
