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

include(cmake/FirmwareSources.cmake)
include(cmake/CompilerDiagnostics.cmake)

set(CARVERA_FIRMWARE_INCLUDE_DIRS
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
  ${FIRMWARE_SRC}/modules/tools/laser
  ${FIRMWARE_SRC}/modules/tools/zprobe
  ${FIRMWARE_SRC}/modules/tools/temperaturecontrol
  ${FIRMWARE_SRC}/modules/tools/temperatureswitch
  ${FIRMWARE_SRC}/modules/tools/drillingcycles
)

set(CARVERA_FIRMWARE_COMPILE_DEFINITIONS
  # Mirror build/build.sh -> make AXIS=5 PAXIS=3 CNC=1, plus the
  # release-profile defines emitted by src/makefile/build/common.mk.
  __GITVERSIONSTRING__="2.1.0c-simulator"
  DEFAULT_SERIAL_BAUD_RATE=115200
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

add_library(carvera_firmware_objects OBJECT ${CARVERA_FIRMWARE_SOURCES})
target_include_directories(carvera_firmware_objects PRIVATE include)
target_include_directories(carvera_firmware_objects SYSTEM PRIVATE ${CARVERA_FIRMWARE_INCLUDE_DIRS})
target_compile_definitions(carvera_firmware_objects PRIVATE
  CHECKSUM_USE_CPP
  ${CARVERA_FIRMWARE_COMPILE_DEFINITIONS}
)
sim_force_include(carvera_firmware_objects ${HOST_PRELUDE})
sim_enable_instrumentation(carvera_firmware_objects)

add_library(carvera_sim_host_objects OBJECT
  ${SIM_FIRMWARE_FACADE_SOURCES}
  ${SIM_CORE_SOURCES}
  ${SIM_PROTOCOL_SOURCES}
  ${SIM_RUNTIME_SUPPORT_SOURCES}
)
target_include_directories(carvera_sim_host_objects PRIVATE include src)
target_include_directories(carvera_sim_host_objects SYSTEM PRIVATE ${CARVERA_FIRMWARE_INCLUDE_DIRS})
target_compile_definitions(carvera_sim_host_objects PRIVATE
  CHECKSUM_USE_CPP
  ${CARVERA_FIRMWARE_COMPILE_DEFINITIONS}
)
sim_force_include(carvera_sim_host_objects ${HOST_PRELUDE})
sim_enable_project_diagnostics(carvera_sim_host_objects)

add_library(carvera_firmware_sim STATIC
  $<TARGET_OBJECTS:carvera_firmware_objects>
  $<TARGET_OBJECTS:carvera_sim_host_objects>
)
target_include_directories(carvera_firmware_sim PUBLIC include)
target_include_directories(carvera_firmware_sim SYSTEM PUBLIC ${CARVERA_FIRMWARE_INCLUDE_DIRS})
target_compile_definitions(carvera_firmware_sim PUBLIC CHECKSUM_USE_CPP)
sim_enable_instrumentation(carvera_firmware_sim)

set_property(
  SOURCE
  ${FIRMWARE_SRC}/modules/utils/simpleshell/SimpleShell.cpp
  APPEND PROPERTY COMPILE_OPTIONS
  "-include;${CMAKE_CURRENT_SOURCE_DIR}/include/sim/dirent_compat.h"
  "$<$<PLATFORM_ID:Windows>:-fpermissive>"
)

add_library(carvera_sim_runtime
  src/runtime/firmware_runtime.cpp
  src/runtime/runtime_boot_session.cpp
  src/runtime/runtime_io.cpp
  src/runtime/runtime_physical_controls.cpp
  src/runtime/runtime_pump.cpp
  src/runtime/simulation_instance.cpp
)
target_include_directories(carvera_sim_runtime PUBLIC include)
target_link_libraries(carvera_sim_runtime PUBLIC carvera_firmware_sim)
sim_force_include(carvera_sim_runtime ${HOST_PRELUDE})
sim_enable_project_diagnostics(carvera_sim_runtime)

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

add_library(carvera_sim_api
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
  src/protocol/machine_state_proto.cpp
  src/protocol/nonblocking_fd_pump.cpp
  src/protocol/api_service.cpp
  src/protocol/framed_proto.cpp
  ${SIM_PLATFORM_API_SOURCES}
)
target_include_directories(carvera_sim_api PUBLIC include)
target_link_libraries(carvera_sim_api PUBLIC carvera_sim_runtime carvera_sim_proto)
if(WIN32)
  target_link_libraries(carvera_sim_api PRIVATE ws2_32)
endif()
sim_force_include(carvera_sim_api ${HOST_PRELUDE})
sim_enable_project_diagnostics(carvera_sim_api)

include(cmake/TestsAndApps.cmake)
