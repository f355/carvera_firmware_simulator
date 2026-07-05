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
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include "sim/delay_hooks.hpp"
#include "sim/interactive_io.hpp"
#include "sim/simulation_instance.hpp"
#include "support/assertions.hpp"
#include "support/posix_io.hpp"
#include "support/temp_sdcard.hpp"
#include "support/xmodem.hpp"

#include <unistd.h>

namespace {

using sim::test::require;

template <typename Pump>
std::string send_and_read_until(int fd, const char* command, const std::string& needle, Pump&& pump,
                                std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  require(sim::test::write_exact(fd, command, std::strlen(command)), "controller command should write");
  return sim::test::read_until_pumping(fd, needle, pump, timeout);
}

template <typename Pump>
std::string poll_status_until(int fd, const std::string& state, Pump&& pump,
                              std::chrono::seconds timeout = std::chrono::seconds(5)) {
  std::string output;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline && output.find(state) == std::string::npos) {
    output += send_and_read_until(fd, "?\n", "MPos:", pump, std::chrono::milliseconds(500));
  }
  return output;
}

void require_no_hard_limit(const std::string& response, const char* message) {
  require(response.find("Hard limit") == std::string::npos && response.find("Limit switch") == std::string::npos,
          message);
}

}  // namespace

int main() {
  sim::test::TempSdCard sd("carvera_sim_controller_handshake_test");
  sd.write_config_txt(
      "# controller handshake config\n"
      "sd_ok true\n"
      "soft_endstop.enable true\n");
  sim::SimulationInstance simulation(sd.persistent_config());
  auto& runtime = simulation.firmware();
  runtime.boot();
  sim::LocalhostTcpBridge bridge(runtime.io(), [&runtime]() { return runtime.is_uploading(); });
  require(bridge.start(0), "localhost WiFi bridge should start");

  std::atomic_bool running{true};
  std::thread pump([&] {
    while (running.load()) {
      bridge.poll();
      {
        sim::delay_hooks::ScopedCallback delay_io_pump([&] { bridge.poll(); });
        runtime.runner().pump_free_running();
      }
      bridge.poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });
  auto wait_for_bridge_io = [] { std::this_thread::yield(); };

  int client = -1;
  require(sim::test::connect_loopback(bridge.port(), client),
          "controller client should connect to localhost WiFi bridge");

  const auto status = send_and_read_until(client, "?\n", "MPos:", wait_for_bridge_io);
  require(status.find("<Idle") != std::string::npos, "controller status query should return firmware status");

  const char download[] = "download /sd/config.txt\n";
  require(sim::test::write_exact(client, download, std::strlen(download)), "config download command should write");
  const auto config = sim::test::receive_xmodem_download(client);
  require(config.find("# controller handshake config") != std::string::npos,
          "controller config download should return host SD config.txt");
  const auto download_done = sim::test::read_until_pumping(client, "download success", wait_for_bridge_io);
  require(download_done.find("download success") != std::string::npos,
          "firmware should finish config download cleanly");

  constexpr char upload_command[] = "upload /sd/gcodes/controller-upload.nc\n";
  constexpr std::string_view upload_contents = "G0 X1 Y2\nM2\n";
  require(sim::test::write_exact(client, upload_command, std::strlen(upload_command)),
          "plain-file upload command should write");
  require(sim::test::send_xmodem_upload(client, upload_contents, wait_for_bridge_io),
          "firmware should accept an uncompressed upload on a 64-bit host");
  const auto upload_done = sim::test::read_until_pumping(client, "upload success", wait_for_bridge_io);
  require(upload_done.find("upload success") != std::string::npos,
          "firmware should finish an uncompressed upload cleanly");
  std::ifstream uploaded_file(sd.path() / "gcodes/controller-upload.nc");
  const std::string uploaded_contents{std::istreambuf_iterator<char>(uploaded_file), std::istreambuf_iterator<char>()};
  require(uploaded_contents == upload_contents, "uncompressed upload should be written to the requested SD path");

  const auto sync_response = send_and_read_until(client, "time\nmodel\nversion\n", "version =", wait_for_bridge_io);
  require(sync_response.find("time =") != std::string::npos, "controller sync should return firmware time");
  require(sync_response.find("model =") != std::string::npos, "controller sync should return firmware model");
  require(sync_response.find("version =") != std::string::npos, "controller sync should return firmware version");

  require(sim::test::write_exact(client, "$J X10 F10000\n", std::strlen("$J X10 F10000\n")),
          "controller jog should write");
  const auto jog_status = poll_status_until(client, "<Idle", wait_for_bridge_io);
  require(jog_status.find("<Idle") != std::string::npos, "controller jog should return to idle");
  require_no_hard_limit(jog_status, "controller jog after startup should not trip a hard limit");

  require(sim::test::write_exact(client, "$J X10000 F10000\n", std::strlen("$J X10000 F10000\n")),
          "controller soft-limit jog should write");
  const auto soft_limit_status = poll_status_until(client, "<Idle", wait_for_bridge_io);
  require_no_hard_limit(soft_limit_status, "controller jog past soft travel should stop before hard switches");
  require(soft_limit_status.find("<Alarm") == std::string::npos,
          "soft-limit rejection should not leave the controller in hard-limit alarm");

  const auto reset_response = send_and_read_until(client, "reset\n", "Rebooting machine", wait_for_bridge_io);
  require(reset_response.find("Rebooting machine") != std::string::npos, "controller reset command should be accepted");

  const auto reboot_sync =
      send_and_read_until(client, "version\n", "version =", wait_for_bridge_io, std::chrono::seconds(8));
  require(reboot_sync.find("version =") != std::string::npos,
          "controller reset test should wait for commands accepted after the actual firmware reboot");

  const auto post_reset_status = poll_status_until(client, "<Idle", wait_for_bridge_io, std::chrono::seconds(8));
  require(post_reset_status.find("<Idle") != std::string::npos,
          "controller reset should reboot firmware back to an idle status");

  require(sim::test::write_exact(client, download, std::strlen(download)),
          "post-reset config download command should write");
  const auto post_reset_config = sim::test::receive_xmodem_download(client);
  require(post_reset_config.find("# controller handshake config") != std::string::npos,
          "controller config download should still work after firmware reset");
  const auto post_reset_download_done = sim::test::read_until_pumping(client, "download success", wait_for_bridge_io);
  require(post_reset_download_done.find("download success") != std::string::npos,
          "post-reset config download should finish cleanly");

  running.store(false);
  pump.join();
  ::close(client);
  return 0;
}
