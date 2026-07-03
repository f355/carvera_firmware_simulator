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

option(CARVERA_SIM_WARNINGS_AS_ERRORS "Treat warnings in simulator-owned code as errors" ON)
option(CARVERA_SIM_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

function(sim_enable_warnings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>
    )
    if(CARVERA_SIM_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
    endif()
  elseif(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
    if(CARVERA_SIM_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  endif()
endfunction()

function(sim_enable_sanitizers target_name)
  if(NOT CARVERA_SIM_ENABLE_SANITIZERS)
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "CARVERA_SIM_ENABLE_SANITIZERS requires Clang or GCC")
  endif()

  target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-fsanitize=address,undefined;-fno-omit-frame-pointer>
    $<$<COMPILE_LANGUAGE:CXX>:-fsanitize=address,undefined;-fno-omit-frame-pointer>
  )
  get_target_property(_target_type ${target_name} TYPE)
  if(NOT _target_type STREQUAL "OBJECT_LIBRARY")
    target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

function(sim_enable_project_diagnostics target_name)
  sim_enable_warnings(${target_name})
  sim_enable_sanitizers(${target_name})
endfunction()

function(sim_enable_test_diagnostics target_name)
  sim_enable_project_diagnostics(${target_name})
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Several white-box firmware tests deliberately expose upstream private
    # members before including the firmware headers.
    target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-keyword-macro>)
  endif()
endfunction()
