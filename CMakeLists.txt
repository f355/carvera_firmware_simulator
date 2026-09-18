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

cmake_minimum_required(VERSION 3.22)

project(CarveraFirmwareSimulator LANGUAGES C CXX)

if(WIN32)
  add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
endif()

set(CARVERA_FIRMWARE_ROOT "" CACHE PATH "Path to the Carvera firmware checkout")
set(CARVERA_ALLOW_UNPINNED_FIRMWARE OFF CACHE BOOL "Allow building against a firmware checkout at a non-pinned commit")
if(DEFINED ENV{CARVERA_ALLOW_UNPINNED_FIRMWARE})
  set(CARVERA_ALLOW_UNPINNED_FIRMWARE "$ENV{CARVERA_ALLOW_UNPINNED_FIRMWARE}" CACHE BOOL
    "Allow building against a firmware checkout at a non-pinned commit" FORCE
  )
endif()
if(NOT CARVERA_FIRMWARE_ROOT AND DEFINED ENV{CARVERA_FIRMWARE_ROOT})
  set(CARVERA_FIRMWARE_ROOT "$ENV{CARVERA_FIRMWARE_ROOT}" CACHE PATH "Path to the Carvera firmware checkout" FORCE)
endif()
if(NOT CARVERA_FIRMWARE_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/Carvera_Community_Firmware/src/main.cpp")
  set(CARVERA_FIRMWARE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/Carvera_Community_Firmware"
    CACHE PATH "Path to the Carvera firmware checkout" FORCE
  )
endif()
if(NOT CARVERA_FIRMWARE_ROOT)
  message(FATAL_ERROR
    "Set CARVERA_FIRMWARE_ROOT to a firmware checkout, "
    "or run scripts/ensure_firmware_checkout.sh to clone the pinned compatible revision"
  )
endif()
get_filename_component(CARVERA_FIRMWARE_ROOT "${CARVERA_FIRMWARE_ROOT}" ABSOLUTE)
set(FIRMWARE_SRC "${CARVERA_FIRMWARE_ROOT}/src")
if(NOT EXISTS "${FIRMWARE_SRC}/main.cpp")
  message(FATAL_ERROR "CARVERA_FIRMWARE_ROOT does not look like a firmware checkout: ${CARVERA_FIRMWARE_ROOT}")
endif()

set(SIM_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/proto/carvera_sim.proto)
find_package(protobuf CONFIG QUIET)
if(TARGET protobuf::libprotobuf)
  set(SIM_PROTOBUF_LIBRARY protobuf::libprotobuf)
  set(SIM_PROTOBUF_CONFIG_MODE ON)
elseif(TARGET Protobuf::libprotobuf)
  set(SIM_PROTOBUF_LIBRARY Protobuf::libprotobuf)
  set(SIM_PROTOBUF_CONFIG_MODE ON)
else()
  find_package(Protobuf REQUIRED)
  set(SIM_PROTOBUF_LIBRARY ${Protobuf_LIBRARIES})
  set(SIM_PROTOBUF_INCLUDE_DIRS ${Protobuf_INCLUDE_DIRS})
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/firmware/compatible_commit" SIM_COMPATIBLE_FIRMWARE_COMMIT)
string(STRIP "${SIM_COMPATIBLE_FIRMWARE_COMMIT}" SIM_COMPATIBLE_FIRMWARE_COMMIT)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CARVERA_FIRMWARE_ROOT}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CARVERA_FIRMWARE_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE SIM_FIRMWARE_HEAD
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE SIM_FIRMWARE_HEAD_RESULT
  )
  if(SIM_FIRMWARE_HEAD_RESULT EQUAL 0
      AND NOT SIM_FIRMWARE_HEAD STREQUAL SIM_COMPATIBLE_FIRMWARE_COMMIT
      AND NOT CARVERA_ALLOW_UNPINNED_FIRMWARE)
    message(FATAL_ERROR
      "CARVERA_FIRMWARE_ROOT is at ${SIM_FIRMWARE_HEAD}, but this simulator is pinned to "
      "${SIM_COMPATIBLE_FIRMWARE_COMMIT}. Re-run scripts/ensure_firmware_checkout.sh, or configure with "
      "-DCARVERA_ALLOW_UNPINNED_FIRMWARE=ON for firmware development."
    )
  endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(HOST_PRELUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/sim/host_prelude.hpp)
if(SIM_PROTOBUF_CONFIG_MODE)
  protobuf_generate(
    LANGUAGE cpp
    OUT_VAR SIM_PROTO_GENERATED
    PROTOS ${SIM_PROTO}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto
  )
  set(SIM_PROTO_SRCS ${SIM_PROTO_GENERATED})
else()
  protobuf_generate_cpp(SIM_PROTO_SRCS SIM_PROTO_HDRS ${SIM_PROTO})
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(SIM_LPC_MEMORY_LAYOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/firmware/lpc_memory_layout.json")
file(READ "${SIM_LPC_MEMORY_LAYOUT_FILE}" SIM_LPC_MEMORY_LAYOUT_JSON)
string(JSON SIM_LPC_LAYOUT_SCHEMA_VERSION GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" schema_version)
if(NOT SIM_LPC_LAYOUT_SCHEMA_VERSION EQUAL 2)
  message(FATAL_ERROR
    "${SIM_LPC_MEMORY_LAYOUT_FILE} has unsupported schema version ${SIM_LPC_LAYOUT_SCHEMA_VERSION}"
  )
endif()
string(JSON SIM_LPC_LAYOUT_FIRMWARE_COMMIT GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" firmware_commit)
if(NOT SIM_LPC_LAYOUT_FIRMWARE_COMMIT STREQUAL SIM_COMPATIBLE_FIRMWARE_COMMIT)
  message(FATAL_ERROR
    "${SIM_LPC_MEMORY_LAYOUT_FILE} describes firmware ${SIM_LPC_LAYOUT_FIRMWARE_COMMIT}, but the simulator is "
    "pinned to ${SIM_COMPATIBLE_FIRMWARE_COMMIT}. Rebuild the firmware and regenerate the layout with "
    "scripts/lpc_memory_layout.py."
  )
endif()
foreach(layout_value IN ITEMS
    ram_start ram_end static_end stack_top stack_limit heap_start heap_end)
  string(JSON SIM_LPC_${layout_value} GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" main_sram ${layout_value})
endforeach()
foreach(layout_value IN ITEMS region_start region_end static_end heap_start heap_end)
  string(JSON SIM_LPC_AHB_${layout_value} GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" ahb_sram ${layout_value})
endforeach()
foreach(layout_value IN ITEMS alignment_bytes header_bytes region_sentinel_bytes)
  string(JSON SIM_LPC_ALLOCATOR_${layout_value} GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" allocator ${layout_value})
endforeach()
# The JSON is the source of truth for tracked types; the template maps each
# entry to its generated constant name.
string(JSON SIM_LPC_TYPE_COUNT LENGTH "${SIM_LPC_MEMORY_LAYOUT_JSON}" type_sizes)
math(EXPR SIM_LPC_TYPE_LAST_INDEX "${SIM_LPC_TYPE_COUNT} - 1")
foreach(type_index RANGE ${SIM_LPC_TYPE_LAST_INDEX})
  string(JSON type_name MEMBER "${SIM_LPC_MEMORY_LAYOUT_JSON}" type_sizes ${type_index})
  string(JSON SIM_LPC_TYPE_${type_name} GET "${SIM_LPC_MEMORY_LAYOUT_JSON}" type_sizes ${type_name})
endforeach()
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lpc_memory_layout.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/lpc_memory_layout.hpp
  @ONLY
)

include(cmake/FirmwareSources.cmake)
include(cmake/CompilerDiagnostics.cmake)

set(CARVERA_FIRMWARE_INCLUDE_DIRS
  ${_firmware_override_include}
  ${CARVERA_FIRMWARE_ROOT}/mbed/src/capi
  ${FIRMWARE_SRC}
  ${FIRMWARE_SRC}/libs
  ${FIRMWARE_SRC}/libs/ConfigSources
  ${FIRMWARE_SRC}/libs/USBDevice
  ${FIRMWARE_SRC}/modules/communication
  ${FIRMWARE_SRC}/modules/communication/utils
  ${FIRMWARE_SRC}/modules/utils/configurator
  ${FIRMWARE_SRC}/modules/utils/mainbutton
  ${FIRMWARE_SRC}/modules/utils/player
  ${FIRMWARE_SRC}/modules/utils/simpleshell
  ${FIRMWARE_SRC}/modules/utils/wifi
  ${FIRMWARE_SRC}/modules/robot
  ${FIRMWARE_SRC}/modules/robot/arm_solutions
  ${FIRMWARE_SRC}/modules/tools/endstops
  ${FIRMWARE_SRC}/modules/tools/switch
  ${FIRMWARE_SRC}/modules/tools/spindle
  ${FIRMWARE_SRC}/modules/tools/toolmanager
  ${FIRMWARE_SRC}/modules/tools/atc
  ${FIRMWARE_SRC}/modules/tools/accessories
  ${FIRMWARE_SRC}/modules/tools/laser
  ${FIRMWARE_SRC}/modules/tools/zprobe
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol
  ${FIRMWARE_SRC}/modules/tools/temperatureswitch
  ${FIRMWARE_SRC}/modules/tools/drillingcycles
  ${FIRMWARE_SRC}/modules/tools/canopen
)

set(SIM_FIRMWARE_COMMON_DEFINITIONS
  # Mirror build/build.sh -> make AXIS=5 PAXIS=3 CNC=1, plus the
  # release-profile defines emitted by src/makefile/build/common.mk.
  __GITVERSIONSTRING__="2.1.0c-simulator"
  CNC
  MAX_ROBOT_ACTUATORS=5
  N_PRIMARY_AXIS=3
  TARGET_LPC1768
  __LPC17XX__
  MRI_ENABLE=0
  MRI_INIT_PARAMETERS="MRI_UART_2 MRI_UART_BAUD=115200"
  MRI_BREAK_ON_INIT=0
  MRI_SEMIHOST_STDIO=0
  WRITE_BUFFER_DISABLE=0
  STACK_SIZE=4096
  __STACK_SIZE=4096
  NOPLAN9
  NO_TOOLS_FILAMENTDETECTOR
  NO_TOOLS_SCARACAL
  NO_TOOLS_EXTRUDER
  NO_TOOLS_ROTARYDELTACALIBRATION
  NO_MODBUS_SPINDLE=1
  NO_VESC_SPINDLE=1
  CARTGRID_ONLY=1
  CARTESIAN_ONLY=1
  NO_PID_AUTOTUNE=1
)

set(SIM_CARVERA_FIRMWARE_DEFINITIONS
  ${SIM_FIRMWARE_COMMON_DEFINITIONS}
  DEFAULT_SERIAL_BAUD_RATE=115200
  MACHINE_FAMILY_CARVERA
)

set(SIM_Z1_FIRMWARE_DEFINITIONS
  ${SIM_FIRMWARE_COMMON_DEFINITIONS}
  DEFAULT_SERIAL_BAUD_RATE=230400
  MACHINE_FAMILY_Z1
  REMOTE_SETTINGS_TRANSFER
  STREAMED_JOB_PLAYBACK
  CANOPEN_SUPPORT
  NO_USB_HOST
  NO_SD_CARD
  NO_WIRELESS_PROBE
  NO_WIFI_PROVIDER
)

function(sim_force_include target_name)
  set(_include_flags)
  foreach(_include_path IN LISTS ARGN)
    string(APPEND _include_flags " -include${_include_path}")
  endforeach()
  string(STRIP "${_include_flags}" _include_flags)
  target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:SHELL:${_include_flags}>
  )
endfunction()

set_property(
  SOURCE
  ${_simpleshell_host_source}
  APPEND PROPERTY COMPILE_OPTIONS
  "-include;${CMAKE_CURRENT_SOURCE_DIR}/include/sim/dirent_compat.h"
  "$<$<PLATFORM_ID:Windows>:-fpermissive>"
)

function(sim_add_firmware_family family firmware_objects host_objects firmware_sim runtime)
  set(family_definitions ${ARGN})
  set(firmware_sources ${CARVERA_FIRMWARE_SOURCES})
  if(family STREQUAL "z1")
    list(APPEND firmware_sources ${Z1_FIRMWARE_ADDITIONAL_SOURCES})
  endif()

  add_library(${firmware_objects} OBJECT ${firmware_sources})
  target_include_directories(${firmware_objects} PRIVATE include)
  target_include_directories(${firmware_objects} SYSTEM PRIVATE ${CARVERA_FIRMWARE_INCLUDE_DIRS})
  if(MSVC)
    target_compile_options(${firmware_objects} PRIVATE /J)
  else()
    target_compile_options(${firmware_objects} PRIVATE -funsigned-char)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${firmware_objects} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-finstrument-functions>)
  endif()
  target_compile_definitions(${firmware_objects} PRIVATE CHECKSUM_USE_CPP ${family_definitions})
  sim_force_include(${firmware_objects} ${HOST_PRELUDE})
  sim_enable_instrumentation(${firmware_objects})

  add_library(${host_objects} OBJECT
    ${SIM_FIRMWARE_FACADE_SOURCES}
    ${SIM_CORE_SOURCES}
    ${SIM_PROTOCOL_SOURCES}
    ${SIM_RUNTIME_SUPPORT_SOURCES}
  )
  target_include_directories(${host_objects} PRIVATE include src ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_include_directories(${host_objects} SYSTEM PRIVATE ${CARVERA_FIRMWARE_INCLUDE_DIRS})
  target_compile_definitions(${host_objects} PRIVATE CHECKSUM_USE_CPP ${family_definitions})
  sim_force_include(${host_objects} ${HOST_PRELUDE})
  sim_enable_project_diagnostics(${host_objects})

  add_library(${firmware_sim} STATIC $<TARGET_OBJECTS:${firmware_objects}> $<TARGET_OBJECTS:${host_objects}>)
  target_include_directories(${firmware_sim} PUBLIC include)
  target_include_directories(${firmware_sim} SYSTEM PUBLIC ${CARVERA_FIRMWARE_INCLUDE_DIRS})
  target_link_libraries(${firmware_sim} PUBLIC ${CMAKE_DL_LIBS})
  if(WIN32)
    target_link_libraries(${firmware_sim} PUBLIC dbghelp)
    if(MINGW)
      target_link_options(${firmware_sim} INTERFACE -Wl,--export-all-symbols)
    endif()
  elseif(NOT APPLE)
    target_link_options(${firmware_sim} INTERFACE -Wl,--export-dynamic)
  endif()
  target_compile_definitions(${firmware_sim} PUBLIC CHECKSUM_USE_CPP)
  sim_enable_instrumentation(${firmware_sim})

  add_library(${runtime}
    src/runtime/firmware_runtime.cpp
    src/runtime/runtime_boot_session.cpp
    src/runtime/runtime_io.cpp
    src/runtime/runtime_physical_controls.cpp
    src/runtime/runtime_pump.cpp
    src/runtime/simulation_instance.cpp
  )
  target_include_directories(${runtime} PUBLIC include)
  target_include_directories(${runtime} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_compile_definitions(${runtime} PRIVATE ${family_definitions})
  target_link_libraries(${runtime} PUBLIC ${firmware_sim})
  sim_force_include(${runtime} ${HOST_PRELUDE})
  sim_enable_project_diagnostics(${runtime})
endfunction()

sim_add_firmware_family(
  carvera carvera_firmware_objects carvera_sim_host_objects carvera_firmware_sim carvera_sim_runtime
  ${SIM_CARVERA_FIRMWARE_DEFINITIONS}
)
sim_add_firmware_family(
  z1 z1_firmware_objects z1_sim_host_objects z1_firmware_sim z1_sim_runtime
  ${SIM_Z1_FIRMWARE_DEFINITIONS}
)

add_library(carvera_sim_proto ${SIM_PROTO_SRCS})
target_include_directories(carvera_sim_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
if(SIM_PROTOBUF_INCLUDE_DIRS)
  target_include_directories(carvera_sim_proto SYSTEM PUBLIC ${SIM_PROTOBUF_INCLUDE_DIRS})
endif()
target_link_libraries(carvera_sim_proto PUBLIC ${SIM_PROTOBUF_LIBRARY})
sim_enable_instrumentation(carvera_sim_proto)

if(WIN32)
  set(SIM_PLATFORM_API_SOURCES
    src/protocol/platform_io_windows.cpp
    src/protocol/stream_request_pump_windows.cpp
    src/protocol/virtual_com_port_windows.cpp
  )
else()
  set(SIM_PLATFORM_API_SOURCES
    src/protocol/platform_io_posix.cpp
    src/protocol/stream_request_pump_posix.cpp
    src/protocol/virtual_com_port.cpp
  )
endif()

set(SIM_API_SOURCES
  src/protocol/api_conversions.cpp
  src/protocol/api_harness_commands.cpp
  src/protocol/api_lifecycle_commands.cpp
  src/protocol/api_physical_commands.cpp
  src/protocol/api_serial_motion_commands.cpp
  src/protocol/api_snapshot.cpp
  src/protocol/api_snapshot_transport_commands.cpp
  src/protocol/discovery_beacon.cpp
  src/protocol/interactive_transport_manager.cpp
  src/protocol/localhost_tcp_bridge.cpp
  src/protocol/lpc_memory_proto.cpp
  src/protocol/machine_state_proto.cpp
  src/protocol/nonblocking_fd_pump.cpp
  src/protocol/api_service.cpp
  src/protocol/framed_proto.cpp
  ${SIM_PLATFORM_API_SOURCES}
)

function(sim_add_api_family api_target runtime_target)
  add_library(${api_target} ${SIM_API_SOURCES})
  target_include_directories(${api_target} PUBLIC include)
  target_link_libraries(${api_target} PUBLIC ${runtime_target} carvera_sim_proto)
  if(WIN32)
    target_link_libraries(${api_target} PRIVATE ws2_32)
  endif()
  sim_force_include(${api_target} ${HOST_PRELUDE})
  sim_enable_project_diagnostics(${api_target})
endfunction()

sim_add_api_family(carvera_sim_api carvera_sim_runtime)
sim_add_api_family(z1_sim_api z1_sim_runtime)

include(cmake/TestsAndApps.cmake)
