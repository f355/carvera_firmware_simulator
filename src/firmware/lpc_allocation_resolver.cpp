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

#include "sim/lpc_memory_accounting.hpp"

#include <array>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#else
#include <dlfcn.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif

#include "AD8495.h"
#include "Adc.h"
#include "AppendFileStream.h"
#include "CartesianSolution.h"
#include "Gcode.h"
#include "PT100_E3D.h"
#include "Pin.h"
#include "Pwm.h"
#include "SoftPWM.h"
#include "StreamOutputPool.h"
#include "Switch.h"
#include "TemperatureControl.h"
#include "Thermistor.h"
#include "lpc_memory_layout.hpp"
#include "max31855.h"

namespace {

bool contains_any(std::string_view value, std::initializer_list<std::string_view> needles) {
  for (const auto needle : needles) {
    if (value.find(needle) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

template <typename HostType>
std::optional<sim::lpc_memory::ResolvedAllocation> resolve_object(std::size_t host_payload_bytes,
                                                                  std::string_view origin,
                                                                  std::initializer_list<std::string_view> origins,
                                                                  std::size_t target_payload_bytes,
                                                                  std::string_view type_name) {
  if (host_payload_bytes != sizeof(HostType) || !contains_any(origin, origins)) {
    return std::nullopt;
  }
  return sim::lpc_memory::ResolvedAllocation{
      .target_payload_bytes = target_payload_bytes,
      .type_name = std::string(type_name),
      .target_size_exact = true,
  };
}

std::string demangle(const char* name) {
  if (name == nullptr) {
    return {};
  }
#if defined(__GNUC__) || defined(__clang__)
  int status = 0;
  char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status != 0 || demangled == nullptr) {
    return name;
  }
  std::string result(demangled);
  std::free(demangled);
  return result;
#else
  return name;
#endif
}

}  // namespace

namespace sim::lpc_memory {

ResolvedAllocation resolve_generic_main_allocation(std::size_t host_payload_bytes, bool array_allocation,
                                                   std::string_view origin) {
  using namespace generated;

  if (array_allocation &&
      contains_any(origin, {"StreamOutput::printf(", "StreamOutput::printfcmd(", "WifiProvider::printf(",
                            "WifiProvider::printfcmd(", "ZProbe::coordinated_move(", "ATCHandler::rapid_move("})) {
    return {
        .target_payload_bytes = host_payload_bytes,
        .type_name = "char[]",
        .target_size_exact = true,
    };
  }
  if (array_allocation && contains_any(origin, {"CartGridStrategy::"})) {
    return {
        .target_payload_bytes = host_payload_bytes,
        .type_name = "float[]",
        .target_size_exact = true,
    };
  }

  if (const auto result = resolve_object<CartesianSolution>(host_payload_bytes, origin, {"Robot::load_config("},
                                                            kCartesianSolutionBytes, "CartesianSolution")) {
    return *result;
  }
  if (const auto result = resolve_object<StreamOutputPool>(host_payload_bytes, origin, {"Kernel::Kernel("},
                                                           kStreamOutputPoolBytes, "StreamOutputPool")) {
    return *result;
  }
  if (const auto result = resolve_object<Adc>(host_payload_bytes, origin, {"Kernel::Kernel("}, kAdcBytes, "Adc")) {
    return *result;
  }
  if (const auto result =
          resolve_object<Gcode>(host_payload_bytes, origin,
                                {"GcodeDispatch::", "Player::", "SimpleShell::", "ZProbe::"}, kGcodeBytes, "Gcode")) {
    return *result;
  }
  if (const auto result =
          resolve_object<AppendFileStream>(host_payload_bytes, origin, {"GcodeDispatch::", "SimpleShell::"},
                                           kAppendFileStreamBytes, "AppendFileStream")) {
    return *result;
  }
  if (const auto result = resolve_object<Pin>(
          host_payload_bytes, origin,
          {"AnalogSpindleControl::", "Laser::", "PWMSpindleControl::", "Switch::", "WifiProvider::"}, kPinBytes,
          "Pin")) {
    return *result;
  }
  if (const auto result = resolve_object<Pwm>(host_payload_bytes, origin, {"Switch::"}, kPwmBytes, "Pwm")) {
    return *result;
  }
  if (const auto result = resolve_object<SoftPWM>(host_payload_bytes, origin, {"Switch::"}, kSoftPWMBytes, "SoftPWM")) {
    return *result;
  }
  if (const auto result =
          resolve_object<Switch>(host_payload_bytes, origin, {"SwitchPool::"}, kSwitchBytes, "Switch")) {
    return *result;
  }
  if (const auto result = resolve_object<TemperatureControl>(host_payload_bytes, origin, {"TemperatureControlPool::"},
                                                             kTemperatureControlBytes, "TemperatureControl")) {
    return *result;
  }
  if (const auto result = resolve_object<Thermistor>(host_payload_bytes, origin, {"TemperatureControl::"},
                                                     kThermistorBytes, "Thermistor")) {
    return *result;
  }
  if (const auto result =
          resolve_object<Max31855>(host_payload_bytes, origin, {"TemperatureControl::"}, kMax31855Bytes, "Max31855")) {
    return *result;
  }
  if (const auto result =
          resolve_object<AD8495>(host_payload_bytes, origin, {"TemperatureControl::"}, kAD8495Bytes, "AD8495")) {
    return *result;
  }
  if (const auto result = resolve_object<PT100_E3D>(host_payload_bytes, origin, {"TemperatureControl::"},
                                                    kPT100E3DBytes, "PT100_E3D")) {
    return *result;
  }

  return {
      .target_payload_bytes = host_payload_bytes,
      .type_name = "ABI-unresolved @ " + std::string(origin.empty() ? "unknown firmware function" : origin),
      .target_size_exact = false,
  };
}

std::string describe_firmware_function(void* function_address) {
  if (function_address == nullptr) {
    return "unknown firmware function";
  }

#if defined(_WIN32)
  const auto process = GetCurrentProcess();
  static const bool symbols_initialized = [] {
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
  }();
  if (symbols_initialized) {
    struct SymbolBuffer {
      SYMBOL_INFO symbol;
      std::array<char, MAX_SYM_NAME> name;
    } storage{};
    auto* symbol = &storage.symbol;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    if (SymFromAddr(process, reinterpret_cast<DWORD64>(function_address), &displacement, symbol) != FALSE) {
      auto result = demangle(symbol->Name);
      if (displacement != 0) {
        char offset[32]{};
        std::snprintf(offset, sizeof(offset), "+0x%llx", static_cast<unsigned long long>(displacement));
        result += offset;
      }
      return result;
    }
  }
#elif defined(__GNUC__) || defined(__clang__)
  Dl_info info{};
  if (dladdr(function_address, &info) != 0 && info.dli_sname != nullptr) {
    auto result = demangle(info.dli_sname);
    const auto address = reinterpret_cast<std::uintptr_t>(function_address);
    const auto symbol = reinterpret_cast<std::uintptr_t>(info.dli_saddr);
    if (address > symbol) {
      char offset[32]{};
      std::snprintf(offset, sizeof(offset), "+0x%llx", static_cast<unsigned long long>(address - symbol));
      result += offset;
    }
    return result;
  }
#endif

  char address[32]{};
  std::snprintf(address, sizeof(address), "%p", function_address);
  return address;
}

}  // namespace sim::lpc_memory
