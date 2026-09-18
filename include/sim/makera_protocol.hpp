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

#ifndef SIMULATOR_SIM_MAKERA_PROTOCOL_HPP
#define SIMULATOR_SIM_MAKERA_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sim::makera {

enum class PacketType : std::uint8_t {
  FirmwareVersion = 0x71,
  StatusResponse = 0x81,
  DiagnosticResponse = 0x82,
  LoadInfo = 0x83,
  LoadFinish = 0x84,
  LoadError = 0x85,
  NormalInfo = 0x90,
  ControlSingle = 0xa1,
  ControlMulti = 0xa2,
  FileStart = 0xb0,
  FileMd5 = 0xb1,
  FileView = 0xb2,
  FileData = 0xb3,
  FileEnd = 0xb4,
  FileCancel = 0xb5,
  FileRetry = 0xb6,
  PlayStatus = 0xb7,
  FirmwareStart = 0xc1,
  FirmwareView = 0xc2,
  FirmwareData = 0xc3,
  FirmwareEnd = 0xc4,
  FirmwareCancel = 0xc5,
  ConfigStart = 0xd1,
  ConfigView = 0xd2,
  ConfigData = 0xd3,
  ConfigEnd = 0xd4,
  ConfigCancel = 0xd5,
  FactoryStart = 0xe1,
  FactoryView = 0xe2,
  FactoryData = 0xe3,
  FactoryEnd = 0xe4,
  FactoryCancel = 0xe5,
  PlayStart = 0xf1,
  PlayView = 0xf2,
  PlayData = 0xf3,
  PlayEnd = 0xf4,
  PlayCancel = 0xf5,
  PlayGoto = 0xf6,
  PlayGotoProgress = 0xf7,
};

struct Frame {
  PacketType type{};
  std::string payload;
};

std::string encode_frame(PacketType type, std::string_view payload);
std::string encode_console_input(std::string_view input);

class FrameDecoder {
 public:
  void append(std::string_view bytes);
  std::vector<Frame> take_frames();
  std::string take_text();
  void reset();

 private:
  void decode_available();

  std::string pending_;
  std::string text_;
  std::vector<Frame> frames_;
};

}  // namespace sim::makera

#endif
