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

#include <string>
#include <string_view>
#include <vector>

#include "Gcode.h"
#include "StreamOutput.h"
#include "support/assertions.hpp"

namespace {

using sim::test::require;
using sim::test::require_near;

struct ExpressionCase {
  std::string_view expression;
  double expected;
  double tolerance{1.0e-5};
};

double evaluate(std::string_view expression) {
  std::vector<char> mutable_expression(expression.begin(), expression.end());
  mutable_expression.push_back('\0');
  char* end = nullptr;
  const double value = Gcode::evaluate_standalone_expression(mutable_expression.data(), &end,
                                                              &StreamOutput::NullStream);
  require(end == mutable_expression.data() + expression.size(),
          std::string("G-code expression parser should consume: ") + std::string(expression));
  return value;
}

}  // namespace

int main() {
  const std::vector<ExpressionCase> cases = {
      {"2 + 3 * 4", 14.0},
      {"[2 + 3]^2", 25.0},
      {"2^3^2", 512.0},
      {"17 mod 5", 2.0},
      {"5 eq 5.0000001", 1.0},
      {"5 ne 6", 1.0},
      {"6 gt 5", 1.0},
      {"5 ge 5", 1.0},
      {"4 lt 5", 1.0},
      {"5 le 5", 1.0},
      {"1 and 2", 1.0},
      {"0 or 3", 1.0},
      {"1 xor 0", 1.0},
      {"0 nor 0", 1.0},
      {"sin[30]", 0.5},
      {"cos[60]", 0.5},
      {"tan[45]", 1.0},
      {"asin[0.5]", 30.0},
      {"acos[0.5]", 60.0},
      {"atan[1]", 45.0},
      {"sqrt[81]", 9.0},
      {"abs[-7.5]", 7.5},
      {"round[2.6]", 3.0},
      {"fix[2.9]", 2.0},
      {"fup[2.1]", 3.0},
      {"ln[exp[2]]", 2.0},
  };

  for (const auto& test_case : cases) {
    require_near(evaluate(test_case.expression), test_case.expected, test_case.tolerance,
                 std::string("unexpected result for expression: ") + std::string(test_case.expression));
  }

  Gcode probe_move("G38.2 X[1 + 2] Y-4 F600 P7", &StreamOutput::NullStream, true, 42);
  require(probe_move.has_g && probe_move.g == 38 && probe_move.subcode == 2,
          "G-code parser should cache command and subcode values");
  require(probe_move.line == 42, "G-code parser should preserve the source line number");
  require(probe_move.get_num_args() == 4, "stripped G-code should expose only its argument letters");
  require_near(probe_move.get_value('X'), 3.0, 1.0e-6, "G-code arguments should accept expressions");
  require_near(probe_move.get_value('Y'), -4.0, 1.0e-6, "G-code arguments should accept signed values");
  require(probe_move.get_int('P') == 7, "G-code parser should expose integer arguments");
  require(probe_move.get_uint('F') == 600, "G-code parser should expose unsigned arguments");
  require(probe_move.get_args().size() == 4, "G-code parser should collect floating-point arguments");
  require(probe_move.get_args_int().size() == 4, "G-code parser should collect integer arguments");

  Gcode motion("G1 X10 Y20 Z-1.5 I0 J1 K2 F300", &StreamOutput::NullStream);
  motion.strip_parameters();
  require(!motion.has_letter('X') && !motion.has_letter('Y') && !motion.has_letter('Z'),
          "motion parameter stripping should remove Cartesian coordinates");
  require(!motion.has_letter('I') && !motion.has_letter('J') && !motion.has_letter('K'),
          "motion parameter stripping should remove arc offsets");
  require(motion.has_letter('F') && motion.get_int('F') == 300,
          "motion parameter stripping should preserve non-coordinate arguments");

  Gcode copied(probe_move);
  require(std::string(copied.get_command()) == probe_move.get_command(),
          "copied G-code should preserve the parsed command text");
  require(copied.has_g && copied.g == probe_move.g && copied.subcode == probe_move.subcode,
          "copied G-code should preserve cached command metadata");

  return 0;
}
