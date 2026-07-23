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

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#else
#include <dlfcn.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace {

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

std::string describe_uncached(void* function_address) {
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

}  // namespace

namespace sim::lpc_memory {

std::string describe_firmware_function(void* function_address) {
  static std::mutex cache_mutex;
  static std::unordered_map<void*, std::string> cache;

  const std::lock_guard lock(cache_mutex);
  const auto existing = cache.find(function_address);
  if (existing != cache.end()) {
    return existing->second;
  }
  auto description = describe_uncached(function_address);
  cache.emplace(function_address, description);
  return description;
}

}  // namespace sim::lpc_memory
