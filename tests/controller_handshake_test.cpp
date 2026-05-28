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

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "sim/delay_hooks.hpp"
#include "sim/firmware_runtime.hpp"
#include "sim/interactive_io.hpp"
#include "sim/machine_simulator.hpp"
#include "support/assertions.hpp"
#include "support/posix_io.hpp"
#include "support/temp_sdcard.hpp"
#include "support/xmodem.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

using sim::test::require;

#ifndef _WIN32
std::string send_and_read_until(int fd, const char* command, const std::string& needle) {
  require(sim::test::write_exact(fd, command, std::strlen(command)), "controller command should write");
  return sim::test::read_until(fd, needle);
}

std::string poll_status_until(int fd, const std::string& state,
                              std::chrono::seconds timeout = std::chrono::seconds(5)) {
  std::string output;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline && output.find(state) == std::string::npos) {
    output += send_and_read_until(fd, "?\n", "MPos:");
  }
  return output;
}

void require_no_hard_limit(const std::string& response, const char* message) {
  require(response.find("Hard limit") == std::string::npos && response.find("Limit switch") == std::string::npos,
          message);
}
#endif

}  // namespace

int main() {
#ifdef _WIN32
  return 0;
#else
  sim::test::TempSdCard sd("carvera_sim_controller_handshake_test");
  sd.write_config_txt(
      "# controller handshake config\n"
      "sd_ok true\n"
      "soft_endstop.enable true\n");
  sd.mount();

  sim::MachineSimulator simulator;
  sim::FirmwareRuntime runtime(simulator);
  runtime.boot();
  sim::LocalhostTcpBridge bridge(runtime);
  require(bridge.start(0), "localhost WiFi bridge should start");

  std::atomic_bool running{true};
  std::thread pump([&] {
    while (running.load()) {
      bridge.poll();
      {
        sim::delay_hooks::ScopedCallback delay_io_pump([&] { bridge.poll(); });
        runtime.pump_free_running();
      }
      bridge.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });

  int client = -1;
  require(sim::test::connect_loopback(bridge.port(), client),
          "controller client should connect to localhost WiFi bridge");

  const auto status = send_and_read_until(client, "?\n", "MPos:");
  require(status.find("<Idle") != std::string::npos, "controller status query should return firmware status");

  const char download[] = "download /sd/config.txt\n";
  require(sim::test::write_exact(client, download, std::strlen(download)), "config download command should write");
  const auto config = sim::test::receive_xmodem_download(client);
  require(config.find("# controller handshake config") != std::string::npos,
          "controller config download should return host SD config.txt");
  const auto download_done = sim::test::read_until(client, "download success");
  require(download_done.find("download success") != std::string::npos,
          "firmware should finish config download cleanly");

  const auto sync_response = send_and_read_until(client, "time\nmodel\nversion\n", "version =");
  require(sync_response.find("time =") != std::string::npos, "controller sync should return firmware time");
  require(sync_response.find("model =") != std::string::npos, "controller sync should return firmware model");
  require(sync_response.find("version =") != std::string::npos, "controller sync should return firmware version");

  require(sim::test::write_exact(client, "$J X10 F10000\n", std::strlen("$J X10 F10000\n")),
          "controller jog should write");
  const auto jog_status = poll_status_until(client, "<Idle");
  require(jog_status.find("<Idle") != std::string::npos, "controller jog should return to idle");
  require_no_hard_limit(jog_status, "controller jog after startup should not trip a hard limit");

  require(sim::test::write_exact(client, "$J X10000 F10000\n", std::strlen("$J X10000 F10000\n")),
          "controller soft-limit jog should write");
  const auto soft_limit_status = poll_status_until(client, "<Idle");
  require_no_hard_limit(soft_limit_status, "controller jog past soft travel should stop before hard switches");
  require(soft_limit_status.find("<Alarm") == std::string::npos,
          "soft-limit rejection should not leave the controller in hard-limit alarm");

  const auto reset_response = send_and_read_until(client, "reset\n", "Rebooting machine");
  require(reset_response.find("Rebooting machine") != std::string::npos, "controller reset command should be accepted");

  const auto reset_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < reset_deadline) {
    require(sim::test::write_exact(client, "?", 1), "controller status poll during reset should write");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  const auto reboot_sync = send_and_read_until(client, "version\n", "version =");
  require(reboot_sync.find("version =") != std::string::npos,
          "controller reset test should wait for commands accepted after the actual firmware reboot");

  const auto post_reset_status = poll_status_until(client, "<Idle", std::chrono::seconds(8));
  require(post_reset_status.find("<Idle") != std::string::npos,
          "controller reset should reboot firmware back to an idle status");

  require(sim::test::write_exact(client, download, std::strlen(download)),
          "post-reset config download command should write");
  const auto post_reset_config = sim::test::receive_xmodem_download(client);
  require(post_reset_config.find("# controller handshake config") != std::string::npos,
          "controller config download should still work after firmware reset");
  const auto post_reset_download_done = sim::test::read_until(client, "download success");
  require(post_reset_download_done.find("download success") != std::string::npos,
          "post-reset config download should finish cleanly");

  running.store(false);
  pump.join();
  ::close(client);
  return 0;
#endif
}
