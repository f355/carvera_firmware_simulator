/*
 * This file is part of the Carvera Firmware Simulator.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <filesystem>
#include <fstream>
#include <string>

#include "sim/makera_protocol.hpp"
#include "sim/crc16.hpp"
#include "sim/z1_mainboard.hpp"
#include "support/assertions.hpp"

using sim::test::require;

namespace {

std::vector<sim::makera::Frame> decode(std::string bytes) {
  sim::makera::FrameDecoder decoder;
  decoder.append(bytes);
  return decoder.take_frames();
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream output(path, std::ios::binary);
  output << contents;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string be32(std::uint32_t value) {
  return {static_cast<char>(value >> 24), static_cast<char>(value >> 16), static_cast<char>(value >> 8),
          static_cast<char>(value)};
}

}  // namespace

int main() {
  using sim::makera::PacketType;
  const auto root = std::filesystem::temp_directory_path() / "carvera-z1-mainboard-test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  write_file(root / "factory.ini", "MachineModel=2\nFuncSetting=0\n");
  write_file(root / "config.txt", "# ignored\nalpha_steps_per_mm 1600\nbeta_steps_per_mm 1600\n");

  sim::Z1Mainboard board(root);

  board.receive_from_host(sim::makera::encode_frame(PacketType::ControlMulti, "G0 X1"));
  auto frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::ControlMulti && frames[0].payload == "G0 X1",
          "unrecognised host commands should be forwarded to the LPC");

  board.receive_from_lpc(sim::makera::encode_frame(PacketType::StatusResponse, "<Idle|C:2,0,0,1>\n"));
  board.receive_from_host(sim::makera::encode_frame(PacketType::ControlSingle, "?"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::StatusResponse,
          "status queries should be answered by the mainboard");
  require(frames[0].payload == "<Idle|C:2,0,0,1|E:0,0,0,0,0|OTA:0,0>\n",
          "status replies should append the mainboard fields");

  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FactoryStart, {}));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FactoryStart,
          "available factory data should acknowledge the start");

  std::string layout(4, '\0');
  layout.push_back('\0');
  layout.push_back(static_cast<char>(132));
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FactoryView, layout));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FactoryView && frames[0].payload.size() == 6 &&
              static_cast<unsigned char>(frames[0].payload[3]) == 2,
          "factory layout should report the available records");

  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FactoryData, be32(1)));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FactoryData &&
              frames[0].payload.substr(4) == "MachineModel=2\n",
          "factory records should be served by one-based index");
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FactoryEnd, {}));
  require(!std::filesystem::exists(root / "factory.ini"),
          "successful factory transfer should consume the one-shot factory file");

  board.receive_from_lpc(sim::makera::encode_frame(PacketType::ConfigView, layout));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::ConfigView &&
              static_cast<unsigned char>(frames[0].payload[4]) == 2 && frames[0].payload[5] == '\0',
          "configuration transfer should force 512-byte frames");

  board.receive_from_lpc(sim::makera::encode_frame(PacketType::ConfigData, be32(1)));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].payload.substr(4) ==
                                    "alpha_steps_per_mm 1600\nbeta_steps_per_mm 1600\n\n",
          "configuration data should aggregate eligible lines");

  write_file(root / "lpc1768.bin", "firmware-image");
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FirmwareStart, {}));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FirmwareStart,
          "an available LPC image should acknowledge firmware transfer start");
  std::string firmware_layout(4, '\0');
  firmware_layout += std::string("\0\x08", 2);
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FirmwareView, firmware_layout));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FirmwareView &&
              frames[0].payload == be32(2) + std::string("\0\x08", 2),
          "firmware layout should be derived from the uploaded image size");
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::FirmwareData, be32(2)));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FirmwareData &&
              frames[0].payload == be32(2) + "-image",
          "firmware transfer should return the requested binary block");

  std::filesystem::create_directories(root / "gcodes");
  write_file(root / "gcodes" / "demo.nc", "G0 X1\nG0 Y2\n");
  board.receive_from_host(sim::makera::encode_frame(PacketType::PlayStatus, "ls /sd/gcodes"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 2 && frames[0].type == PacketType::LoadInfo &&
              frames[0].payload == "demo.nc\r\n" && frames[1].type == PacketType::LoadFinish,
          "the ESP-owned directory listing should be served without involving the LPC");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileStart, "download /sd/gcodes/demo.nc\n"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileMd5 && frames[0].payload.size() == 32,
          "a download should begin with an MD5 announcement");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileView, {}));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileView &&
              frames[0].payload == be32(1) + std::string("\x20\x00", 2),
          "a small download should advertise one 8192-byte frame");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileData, be32(1)));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileData &&
              frames[0].payload == be32(1) + "G0 X1\nG0 Y2\n",
          "download data should contain the requested block");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileEnd, {}));
  frames = decode(board.take_host_tx());
  require(frames.size() == 2 && frames[0].type == PacketType::FileEnd && frames[1].type == PacketType::NormalInfo,
          "download completion should acknowledge and report success");

  board.receive_from_host(sim::makera::encode_frame(PacketType::FileStart, "upload /sd/gcodes/upload.nc\n"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileMd5,
          "an upload should request the file digest");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileMd5, "0123456789abcdef0123456789abcdef"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileView,
          "an upload should request its frame layout after the digest");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileView, be32(1)));
  frames = decode(board.take_host_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::FileData && frames[0].payload == be32(1),
          "an upload should request its first data frame");
  board.receive_from_host(sim::makera::encode_frame(PacketType::FileData, be32(1) + "M3 S100\n"));
  frames = decode(board.take_host_tx());
  require(frames.size() == 2 && frames[0].type == PacketType::FileEnd &&
              read_file(root / "gcodes" / "upload.nc") == "M3 S100\n",
          "the final upload frame should be persisted and acknowledged");

  board.receive_from_host(sim::makera::encode_frame(PacketType::ControlMulti, "play /sd/gcodes/demo.nc"));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::ControlMulti,
          "play preparation should still forward the command to the LPC");
  const auto identifier = sim::crc16_ccitt("/sd/gcodes/demo.nc");
  const std::string play_id{static_cast<char>(identifier >> 8), static_cast<char>(identifier)};
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::PlayStart, play_id));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 1 && frames[0].type == PacketType::PlayView && frames[0].payload.starts_with(play_id),
          "a matching streamed-play start should report the file size");
  board.receive_from_lpc(sim::makera::encode_frame(PacketType::PlayData, play_id + be32(0)));
  frames = decode(board.take_lpc_tx());
  require(frames.size() == 2 && frames[0].type == PacketType::PlayData &&
              frames[0].payload.substr(6) == "G0 X1\nG0 Y2\n" && frames[1].type == PacketType::PlayEnd,
          "streamed play should provide normalized G-code lines and then signal EOF");

  std::filesystem::remove_all(root);
  return 0;
}
