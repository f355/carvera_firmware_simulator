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

#ifndef SIMULATOR_TESTS_SUPPORT_ASSERTIONS_HPP
#define SIMULATOR_TESTS_SUPPORT_ASSERTIONS_HPP

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace sim::test {

inline void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

inline bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

inline void require_contains(std::string_view haystack, std::string_view needle, std::string_view message) {
  if (haystack.find(needle) == std::string_view::npos) {
    std::cerr << message << "\nexpected to find: " << needle << "\nactual:\n" << haystack << '\n';
    std::exit(1);
  }
}

inline void require_near(double actual, double expected, double tolerance, std::string_view message) {
  if (std::abs(actual - expected) > tolerance) {
    std::cerr << message << ": expected " << expected << " +/- " << tolerance << ", got " << actual << '\n';
    std::exit(1);
  }
}

template <typename Actual, typename Expected>
inline void require_equal(const Actual& actual, const Expected& expected, std::string_view message) {
  if (actual != expected) {
    std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
    std::exit(1);
  }
}

}  // namespace sim::test

#endif
