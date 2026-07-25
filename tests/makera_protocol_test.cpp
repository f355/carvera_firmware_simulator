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

#include "sim/makera_protocol.hpp"
#include "support/assertions.hpp"

using sim::test::require;

int main() {
  using sim::makera::FrameDecoder;
  using sim::makera::PacketType;

  const std::string expected{
      '\x86', '\x68', '\x00', '\x06', '\xa2', '\x47',
      '\x39', '\x31', '\xcb', '\xd9', '\x55', '\xaa',
  };
  const auto encoded = sim::makera::encode_console_input("G91\n");
  require(encoded == expected, "Makera encoder should match the firmware wire format");

  FrameDecoder fragmented;
  fragmented.append(encoded.substr(0, 5));
  require(fragmented.take_frames().empty(), "decoder should retain an incomplete frame");
  fragmented.append(encoded.substr(5));
  auto frames = fragmented.take_frames();
  require(frames.size() == 1, "decoder should assemble a fragmented frame");
  require(frames[0].type == PacketType::ControlMulti, "decoder should preserve the packet type");
  require(frames[0].payload == "G91", "console encoder should remove the plaintext line terminator");

  FrameDecoder coalesced;
  coalesced.append(encoded + sim::makera::encode_frame(PacketType::StatusResponse, "<Idle>"));
  frames = coalesced.take_frames();
  require(frames.size() == 2, "decoder should split coalesced frames");
  require(frames[1].type == PacketType::StatusResponse && frames[1].payload == "<Idle>",
          "decoder should preserve the second coalesced frame");
  require(coalesced.take_text() == "G91<Idle>", "text view should concatenate decoded frame payloads");

  FrameDecoder mixed;
  mixed.append(encoded + "raw file data\n" + sim::makera::encode_frame(PacketType::NormalInfo, "ok\n"));
  require(mixed.take_text() == "G91raw file data\nok\n",
          "text view should preserve firmware paths that intentionally write unframed bytes");

  auto corrupted = encoded;
  corrupted[9] ^= 0x01;
  FrameDecoder recovering;
  recovering.append(std::string("noise") + corrupted + encoded);
  frames = recovering.take_frames();
  require(frames.size() == 1 && frames[0].payload == "G91",
          "decoder should reject a bad CRC and recover at the next header");

  const auto query = sim::makera::encode_console_input("?\n");
  FrameDecoder query_decoder;
  query_decoder.append(query);
  frames = query_decoder.take_frames();
  require(frames.size() == 1 && frames[0].type == PacketType::ControlSingle && frames[0].payload == "?",
          "single-byte realtime commands should use Makera control-single frames");

  FrameDecoder commands_decoder;
  commands_decoder.append(sim::makera::encode_console_input("$H\n$J=G91 X1 F1500\n"));
  frames = commands_decoder.take_frames();
  require(frames.size() == 2 && frames[0].payload == "$H" && frames[1].payload == "$J=G91 X1 F1500",
          "each newline-terminated command should use its own Makera control-multi frame");

  return 0;
}
