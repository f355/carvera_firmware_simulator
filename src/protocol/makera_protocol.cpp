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

#include "sim/makera_protocol.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sim::makera {
namespace {

constexpr std::uint16_t kHeader = 0x8668;
constexpr std::uint16_t kFooter = 0x55aa;
constexpr std::size_t kFrameOverhead = 9;
constexpr std::size_t kMinimumDataLength = 3;

std::uint16_t crc16_ccitt(std::string_view bytes) {
  std::uint16_t crc = 0;
  for (const auto byte : bytes) {
    crc ^= static_cast<std::uint16_t>(static_cast<unsigned char>(byte)) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
  }
  return crc;
}

bool is_single_control(std::string_view input) {
  if (input.size() != 1) {
    return false;
  }
  switch (input.front()) {
    case '?':
    case '*':
    case '!':
    case '~':
    case '\x18':
    case '\x19':
    case '\x1a':
      return true;
    default:
      return false;
  }
}

std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
  return static_cast<std::uint16_t>((static_cast<unsigned char>(bytes[offset]) << 8) |
                                    static_cast<unsigned char>(bytes[offset + 1]));
}

}  // namespace

std::string encode_frame(PacketType type, std::string_view payload) {
  constexpr auto kMaximumPayload =
      static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) - kMinimumDataLength;
  if (payload.size() > kMaximumPayload) {
    throw std::length_error("Makera frame payload is too large");
  }

  const auto data_length = static_cast<std::uint16_t>(payload.size() + kMinimumDataLength);
  std::string frame;
  frame.reserve(payload.size() + kFrameOverhead);
  frame.push_back(static_cast<char>(kHeader >> 8));
  frame.push_back(static_cast<char>(kHeader & 0xff));
  frame.push_back(static_cast<char>(data_length >> 8));
  frame.push_back(static_cast<char>(data_length & 0xff));
  frame.push_back(static_cast<char>(type));
  frame.append(payload);
  const auto crc = crc16_ccitt(std::string_view(frame).substr(2));
  frame.push_back(static_cast<char>(crc >> 8));
  frame.push_back(static_cast<char>(crc & 0xff));
  frame.push_back(static_cast<char>(kFooter >> 8));
  frame.push_back(static_cast<char>(kFooter & 0xff));
  return frame;
}

std::string encode_console_input(std::string_view input) {
  if (input.empty()) {
    return {};
  }

  auto control = input;
  while (control.size() > 1 && (control.back() == '\n' || control.back() == '\r')) {
    control.remove_suffix(1);
  }
  if (is_single_control(control)) {
    return encode_frame(PacketType::ControlSingle, control);
  }

  std::string frames;
  std::size_t offset = 0;
  while (offset < input.size()) {
    const auto newline = input.find('\n', offset);
    const auto end = newline == std::string_view::npos ? input.size() : newline;
    auto command = input.substr(offset, end - offset);
    if (!command.empty() && command.back() == '\r') {
      command.remove_suffix(1);
    }
    if (!command.empty()) {
      frames += encode_frame(PacketType::ControlMulti, command);
    }
    offset = newline == std::string_view::npos ? input.size() : newline + 1;
  }
  return frames;
}

void FrameDecoder::append(std::string_view bytes) {
  pending_.append(bytes);
  decode_available();
}

std::vector<Frame> FrameDecoder::take_frames() {
  auto frames = std::move(frames_);
  frames_.clear();
  return frames;
}

std::string FrameDecoder::take_text() {
  auto text = std::move(text_);
  text_.clear();
  return text;
}

void FrameDecoder::reset() {
  pending_.clear();
  text_.clear();
  frames_.clear();
}

void FrameDecoder::decode_available() {
  constexpr char kHeaderHigh = static_cast<char>(kHeader >> 8);
  constexpr char kHeaderLow = static_cast<char>(kHeader & 0xff);

  for (;;) {
    const auto header = pending_.find(std::string{kHeaderHigh, kHeaderLow});
    if (header == std::string::npos) {
      const bool retain_header_prefix = !pending_.empty() && pending_.back() == kHeaderHigh;
      text_.append(pending_.data(), pending_.size() - static_cast<std::size_t>(retain_header_prefix));
      pending_ = retain_header_prefix ? std::string(1, kHeaderHigh) : std::string{};
      return;
    }
    if (header != 0) {
      text_.append(pending_, 0, header);
      pending_.erase(0, header);
    }
    if (pending_.size() < 4) {
      return;
    }

    const auto data_length = static_cast<std::size_t>(read_u16(pending_, 2));
    if (data_length < kMinimumDataLength) {
      pending_.erase(0, 1);
      continue;
    }
    const auto frame_length = data_length + 6;
    if (pending_.size() < frame_length) {
      return;
    }

    const auto received_crc = read_u16(pending_, data_length + 2);
    const auto received_footer = read_u16(pending_, data_length + 4);
    const auto calculated_crc = crc16_ccitt(std::string_view(pending_).substr(2, data_length));
    if (received_crc != calculated_crc || received_footer != kFooter) {
      pending_.erase(0, 1);
      continue;
    }

    auto payload = pending_.substr(5, data_length - kMinimumDataLength);
    text_.append(payload);
    frames_.push_back(Frame{static_cast<PacketType>(static_cast<unsigned char>(pending_[4])), std::move(payload)});
    pending_.erase(0, frame_length);
  }
}

}  // namespace sim::makera
