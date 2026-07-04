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
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "Gcode.h"
#include "OCodeHandler.h"
#include "StreamOutput.h"
#include "libs/FirmwareFileSystem.h"
#include "libs/Kernel.h"
#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"

namespace {

using sim::test::require;
using sim::test::require_near;

struct ExecutedLine {
  std::string command;
  float parameter_1;
  float parameter_2;
};

std::string without_line_ending(const char* line) {
  std::string result(line);
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
    result.pop_back();
  }
  return result;
}

std::vector<ExecutedLine> execute_ocode_program(Kernel& kernel, std::string_view program) {
  for (int index = 0; index < 30; ++index) {
    kernel.local_params[index] = 0.0F;
  }

  FILE* file = std::tmpfile();
  require(file != nullptr, "O-code test should create a temporary program file");
  require(std::fwrite(program.data(), 1, program.size(), file) == program.size(),
          "O-code test should write the complete temporary program");
  std::rewind(file);

  OCodeHandler handler;
  handler.pre_scan(file, &StreamOutput::NullStream);

  std::vector<ExecutedLine> executed;
  std::array<char, 130> line{};
  unsigned long file_line = 0;
  int iterations = 0;
  while (fwfs::fgets(line.data(), line.size(), file) != nullptr) {
    require(++iterations < 1000, "O-code interpreter should terminate without exhausting its loop guard");
    ++file_line;

    if (handler.process_line(line.data(), file, &StreamOutput::NullStream, file_line) || handler.is_skipping()) {
      continue;
    }

    const std::string command = without_line_ending(line.data());
    if (command.empty()) {
      continue;
    }
    if (command.front() == '#') {
      Gcode assignment(command, &StreamOutput::NullStream, false, file_line);
      assignment.set_variable_value();
      continue;
    }
    executed.push_back({command, kernel.local_params[0], kernel.local_params[1]});
  }

  std::fclose(file);
  return executed;
}

void require_commands(const std::vector<ExecutedLine>& lines, const std::vector<std::string>& expected,
                      std::string_view message) {
  require(lines.size() == expected.size(), message);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    require(lines[index].command == expected[index], message);
  }
}

}  // namespace

int main() {
  sim::MachineSimulator simulator;
  Kernel kernel;

  const auto branches = execute_ocode_program(kernel,
                                              "O10 if [0]\n"
                                              "G0 X1\n"
                                              "O10 elseif [1]\n"
                                              "G0 X2\n"
                                              "O10 else\n"
                                              "G0 X3\n"
                                              "O10 endif\n"
                                              "G0 X4\n");
  require_commands(branches, {"G0 X2", "G0 X4"}, "O-code if/elseif/else should execute only the selected branch");

  const auto while_loop = execute_ocode_program(kernel,
                                                "#1=0\n"
                                                "O20 while [#1 lt 2]\n"
                                                "G1 X#1\n"
                                                "#1=[#1+1]\n"
                                                "O20 endwhile\n"
                                                "G1 X9\n");
  require_commands(while_loop, {"G1 X#1", "G1 X#1", "G1 X9"},
                   "O-code while should repeat until its expression becomes false");
  require_near(while_loop[0].parameter_1, 0.0, 1.0e-6, "first while iteration should see the initial parameter");
  require_near(while_loop[1].parameter_1, 1.0, 1.0e-6, "second while iteration should see the updated parameter");

  const auto do_loop = execute_ocode_program(kernel,
                                             "#1=0\n"
                                             "O30 do\n"
                                             "G1 Y#1\n"
                                             "#1=[#1+1]\n"
                                             "O30 while [#1 lt 2]\n");
  require_commands(do_loop, {"G1 Y#1", "G1 Y#1"},
                   "O-code do/while should execute its body before checking the condition");
  require_near(do_loop[1].parameter_1, 1.0, 1.0e-6, "do/while should retain parameter updates across iterations");

  const auto skipped_while = execute_ocode_program(kernel,
                                                   "O35 while [0]\n"
                                                   "G1 X99\n"
                                                   "O36 while [1]\n"
                                                   "G1 X98\n"
                                                   "O36 endwhile\n"
                                                   "O35 endwhile\n"
                                                   "G1 X1\n");
  require_commands(skipped_while, {"G1 X1"}, "a false while should skip its complete body, including nested loops");

  const auto repeat_continue = execute_ocode_program(kernel,
                                                     "O40 repeat [3]\n"
                                                     "G1 X1\n"
                                                     "O40 continue\n"
                                                     "G1 X99\n"
                                                     "O40 endrepeat\n"
                                                     "G1 X2\n");
  require_commands(repeat_continue, {"G1 X1", "G1 X1", "G1 X1", "G1 X2"},
                   "O-code continue should skip the remainder of each repeat iteration");

  const auto repeat_break = execute_ocode_program(kernel,
                                                  "O50 repeat [5]\n"
                                                  "G1 X1\n"
                                                  "O50 break\n"
                                                  "G1 X99\n"
                                                  "O50 endrepeat\n"
                                                  "G1 X2\n");
  require_commands(repeat_break, {"G1 X1", "G1 X2"}, "O-code break should leave the matching repeat loop immediately");

  const auto do_continue = execute_ocode_program(kernel,
                                                 "#1=0\n"
                                                 "O60 do\n"
                                                 "G1 X#1\n"
                                                 "#1=[#1+1]\n"
                                                 "O60 continue\n"
                                                 "G1 X99\n"
                                                 "O60 while [#1 lt 3]\n"
                                                 "G1 X9\n");
  require_commands(do_continue, {"G1 X#1", "G1 X#1", "G1 X#1", "G1 X9"},
                   "continue in a do/while should evaluate its condition and start the next iteration");

  const auto while_break = execute_ocode_program(kernel,
                                                 "O70 while [1]\n"
                                                 "G1 X1\n"
                                                 "O70 break\n"
                                                 "G1 X99\n"
                                                 "O70 endwhile\n"
                                                 "G1 X2\n");
  require_commands(while_break, {"G1 X1", "G1 X2"}, "break should skip to the end of the matching while loop");

  const auto zero_repeat = execute_ocode_program(kernel,
                                                 "O80 repeat [0]\n"
                                                 "G1 X99\n"
                                                 "O80 endrepeat\n"
                                                 "G1 X1\n");
  require_commands(zero_repeat, {"G1 X1"}, "a zero-count repeat should skip its body");

  const auto subroutine = execute_ocode_program(kernel,
                                                "O100 call [7] [8]\n"
                                                "G1 X99\n"
                                                "O100 sub\n"
                                                "G1 X#1 Y#2\n"
                                                "O100 endsub\n");
  require_commands(subroutine, {"G1 X#1 Y#2", "G1 X99"},
                   "O-code call should execute a subroutine before returning to its caller");
  require_near(subroutine[0].parameter_1, 7.0, 1.0e-6, "first subroutine argument should populate parameter #1");
  require_near(subroutine[0].parameter_2, 8.0, 1.0e-6, "second subroutine argument should populate parameter #2");
  require_near(kernel.local_params[0], 0.0, 1.0e-6, "subroutine return should restore caller parameters");

  return 0;
}
