/*
 * This file is part of the Carvera Firmware Simulator.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <filesystem>
#include <fstream>
#include <iostream>

#include "carvera_sim.pb.h"
#include "sim/makera_protocol.hpp"
#include "support/assertions.hpp"
#include "support/stream_stdio_harness.hpp"
#include "support/temp_sdcard.hpp"

using sim::test::expect;

namespace {

bool boot_model(const char* binary, carvera::sim::v1::MachineModel model, std::string_view model_name,
                std::string_view config_name, int factory_model) {
  sim::test::TempSdCard sd("carvera_sim_" + std::string(model_name) + "_backend_test");
  std::filesystem::copy_file(std::filesystem::path(CARVERA_FIRMWARE_ROOT) / "src" / config_name,
                             sd.path() / "config.txt", std::filesystem::copy_options::overwrite_existing);
  {
    std::ofstream factory(sd.path() / "factory.ini");
    factory << "Machine_Model " << factory_model << "\nA_Axis_home_enable 0\nAtc_enable 0\n";
  }

  sim::test::StreamStdioHarness simulator(binary);
  if (!expect(simulator.start(), "failed to start Z1 simulator")) return false;

  carvera::sim::v1::Request request;
  carvera::sim::v1::Response response;
  request.mutable_set_machine_model()->set_machine_model(model);
  request.mutable_set_machine_model()->set_function_setting(0);
  if (!expect(simulator.request_ok(request, 1, response), "set Z1 model failed")) return false;

  request.Clear();
  response.Clear();
  request.mutable_mount_filesystem()->set_name("sd");
  request.mutable_mount_filesystem()->set_host_path(sd.path().string());
  if (!expect(simulator.request_ok(request, 2, response), "mount Z1 SD failed")) return false;

  request.Clear();
  response.Clear();
  request.mutable_get_machine_snapshot();
  if (!expect(simulator.request_ok(request, 3, response, std::chrono::seconds(10)), "Z1 firmware boot failed") ||
      !expect(response.machine_snapshot().firmware_booted(), std::string(model_name) + " firmware should report booted") ||
      !expect(response.machine_snapshot().tool_setter_available(),
              std::string(model_name) + " should expose its calibrated ETS") ||
      !expect(response.machine_snapshot().tool_setter().max_z() == -108.0,
              std::string(model_name) + " ETS should use the calibrated trigger height")) {
    std::cerr << simulator.stderr_output();
    return false;
  }

  request.Clear();
  response.Clear();
  request.mutable_get_status();
  if (!expect(simulator.request_ok(request, 30, response), "Z1 status API failed") ||
      !expect(response.status().machine_model() == model, std::string(model_name) + " should preserve its factory model")) {
    return false;
  }

  request.Clear();
  response.Clear();
  request.mutable_write_serial()->set_data(
      sim::makera::encode_frame(sim::makera::PacketType::ControlSingle, "?"));
  if (!expect(simulator.request_ok(request, 4, response), "Z1 status query write failed")) return false;

  request.Clear();
  response.Clear();
  request.mutable_read_serial();
  if (!expect(simulator.request_ok(request, 5, response), "Z1 status query read failed")) return false;
  sim::makera::FrameDecoder decoder;
  decoder.append(response.serial_data().data());
  const auto frames = decoder.take_frames();
  if (!expect(frames.size() == 1 && frames.front().type == sim::makera::PacketType::StatusResponse,
              "Z1 mainboard should answer controller status queries")) {
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: z1_backend_boot_test <carvera_sim_stream_stdio_z1>\n";
    return 2;
  }
  if (!boot_model(argv[1], carvera::sim::v1::MACHINE_MODEL_MAKERA_Z1, "z1", "config_z1.default", 3)) return 1;
  if (!boot_model(argv[1], carvera::sim::v1::MACHINE_MODEL_MAKERA_Z1_PRO, "z1pro", "config_z1pro.default", 4)) {
    return 1;
  }
  return 0;
}
