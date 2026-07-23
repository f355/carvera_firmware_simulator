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

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "carvera_sim.pb.h"
#include "sim/makera_protocol.hpp"
#include "support/assertions.hpp"
#include "support/framed_proto_client.hpp"
#include "support/temp_sdcard.hpp"

namespace {

bool write_request(int fd, const carvera::sim::v1::Request& request) {
  return sim::test::write_framed_message(fd, request);
}

bool read_response(int fd, carvera::sim::v1::Response& response) {
  return sim::test::read_framed_message(fd, response);
}

void write_minimal_config(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::ofstream config(root / "config");
  config << "protocol makera\n"
         << "arm_solution cartesian\n"
         << "alpha_step_pin 1.28\n"
         << "alpha_dir_pin 1.29\n"
         << "alpha_en_pin nc\n"
         << "alpha_steps_per_mm 200\n"
         << "alpha_max_rate 3000\n"
         << "alpha_acceleration 150\n"
         << "soft_endstop.enable false\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: stdio_api_test <carvera_sim_stdio>\n";
    return 2;
  }

  int to_child[2]{};
  int from_child[2]{};
  if (pipe(to_child) != 0 || pipe(from_child) != 0) {
    std::cerr << "pipe failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  const auto child = fork();
  if (child < 0) {
    std::cerr << "fork failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  if (child == 0) {
    dup2(to_child[0], STDIN_FILENO);
    dup2(from_child[1], STDOUT_FILENO);
    close(to_child[0]);
    close(to_child[1]);
    close(from_child[0]);
    close(from_child[1]);
    execl(argv[1], argv[1], nullptr);
    _exit(127);
  }

  close(to_child[0]);
  close(from_child[1]);

  using sim::test::expect;

  sim::test::TempSdCard sd("carvera_sim_stdio_api_test");
  write_minimal_config(sd.path());

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;

  request.set_id(1);
  request.mutable_get_status();
  if (!expect(write_request(to_child[1], request), "failed to write get_status") ||
      !expect(read_response(from_child[0], response), "failed to read get_status response") ||
      !expect(response.ok(), "get_status failed") || !expect(response.id() == 1, "get_status response id mismatch") ||
      !expect(response.status().time_us() == 0, "initial status time mismatch") ||
      !expect(response.status().time_mode() == carvera::sim::v1::TIME_MODE_MANUAL, "initial status mode mismatch")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(2);
  request.mutable_advance_time()->set_delta_us(77);
  if (!expect(write_request(to_child[1], request), "failed to write advance_time") ||
      !expect(read_response(from_child[0], response), "failed to read advance_time response") ||
      !expect(response.ok(), "advance_time failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(3);
  request.mutable_get_status();
  if (!expect(write_request(to_child[1], request), "failed to write second get_status") ||
      !expect(read_response(from_child[0], response), "failed to read second get_status response") ||
      !expect(response.ok(), "second get_status failed") ||
      !expect(response.status().time_us() == 77, "advanced status time mismatch")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(4);
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(write_request(to_child[1], request), "failed to write mount request") ||
      !expect(read_response(from_child[0], response), "failed to read mount response") ||
      !expect(response.ok(), "mount_filesystem failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(5);
  request.mutable_write_serial()->set_data(sim::makera::encode_console_input("G91\n"));
  if (!expect(write_request(to_child[1], request), "failed to write serial request") ||
      !expect(read_response(from_child[0], response), "failed to read serial write response") ||
      !expect(response.ok(), "write_serial failed")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(6);
  request.mutable_run_until_idle()->set_max_step_ticks(1000);
  if (!expect(write_request(to_child[1], request), "failed to write run_until_idle") ||
      !expect(read_response(from_child[0], response), "failed to read run_until_idle response") ||
      !expect(response.ok(), "run_until_idle failed") ||
      !expect(response.run_result().idle(), "run_until_idle did not report idle")) {
    return 1;
  }

  request.Clear();
  response.Clear();
  request.set_id(7);
  request.mutable_read_serial();
  if (!expect(write_request(to_child[1], request), "failed to write read_serial") ||
      !expect(read_response(from_child[0], response), "failed to read read_serial response") ||
      !expect(response.ok(), "read_serial failed")) {
    return 1;
  }

  close(to_child[1]);
  close(from_child[0]);

  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    std::cerr << "waitpid failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}
