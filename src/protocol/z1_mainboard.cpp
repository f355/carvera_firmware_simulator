/*
 * This file is part of the Carvera Firmware Simulator.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sim/z1_mainboard.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#include "libs/md5.h"
#include "sim/crc16.hpp"

namespace sim {
namespace {

constexpr std::string_view kInitialStatus =
    "<Idle|MPos:-1.0000,-1.0000,-1.0000,0.0000,0.0000|WPos:144.4120,158.7000,77.9550,8.0010,0.0000|"
    "    F:0.0,3000.0,100.0|S:0.0,10000.0,100.0,0,27.0|T:1,-15.180|W:4.13|L:0,0,0,0.0,100.0|"
    "P:1234,50,1200|A:1|O:-1.351|H:1|C:1,5,0,1>\n";

std::uint16_t read_be16(std::string_view bytes, std::size_t offset) {
  return static_cast<std::uint16_t>((static_cast<unsigned char>(bytes[offset]) << 8) |
                                    static_cast<unsigned char>(bytes[offset + 1]));
}

std::uint32_t read_be32(std::string_view bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) << 24) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 8) |
         static_cast<unsigned char>(bytes[offset + 3]);
}

void append_be16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value >> 8));
  bytes.push_back(static_cast<char>(value));
}

void append_be32(std::string& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<char>(value >> 24));
  bytes.push_back(static_cast<char>(value >> 16));
  bytes.push_back(static_cast<char>(value >> 8));
  bytes.push_back(static_cast<char>(value));
}

std::string decode_argument(std::string_view argument) {
  std::string result;
  result.reserve(argument.size());
  for (const char byte : argument) {
    if (byte == '\0') break;
    switch (byte) {
      case '\x01':
        result.push_back(' ');
        break;
      case '\x02':
        result.push_back('?');
        break;
      case '\x03':
        result.push_back('*');
        break;
      case '\x04':
        result.push_back('!');
        break;
      case '\x05':
        result.push_back('~');
        break;
      default:
        result.push_back(byte);
        break;
    }
  }
  while (!result.empty() && result.front() == ' ') result.erase(result.begin());
  while (!result.empty() && (result.back() == ' ' || result.back() == '\r' || result.back() == '\n' ||
                             result.back() == '\0')) {
    result.pop_back();
  }
  return result;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string file_md5(const std::filesystem::path& path) {
  const auto contents = read_file(path);
  return MD5(contents).hexdigest();
}

std::filesystem::path md5_sidecar(const std::filesystem::path& sd_root, std::string_view display_path) {
  constexpr std::string_view gcodes = "/sd/gcodes/";
  constexpr std::string_view sd = "/sd/";
  if (display_path.starts_with(gcodes)) {
    return sd_root / "gcodes" / ".md5" / std::string(display_path.substr(gcodes.size()));
  }
  if (display_path.starts_with(sd)) {
    return sd_root / ".md5" / std::string(display_path.substr(sd.size()));
  }
  return {};
}

std::vector<std::string> play_lines(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    if (line.size() > 63) line.resize(63);
    line.push_back('\n');
    lines.push_back(std::move(line));
  }
  return lines;
}

std::vector<std::string> config_records(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::vector<std::string> records;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() <= 2 || line.front() == '#' || line.front() == '*') continue;
    if (line.size() > 63) line.resize(63);
    line.push_back('\n');
    records.push_back(std::move(line));
  }
  return records;
}

std::vector<std::string> factory_records(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::vector<std::string> records;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() <= 2 || line.front() == '#') continue;
    line.push_back('\n');
    records.push_back(std::move(line));
  }
  return records;
}

}  // namespace

Z1Mainboard::Z1Mainboard(std::filesystem::path sd_root) : sd_root_(std::move(sd_root)) { reset(); }

void Z1Mainboard::reset() {
  uart_decoder_.reset();
  tcp_decoder_.reset();
  lpc_decoder_.reset();
  uart_tx_.clear();
  tcp_tx_.clear();
  lpc_tx_.clear();
  latest_status_ = std::string(kInitialStatus);
  latest_diagnostic_.clear();
  latest_version_.clear();
  file_transfer_ = {};
  play_ = {};
  uart_active_ = false;
  tcp_active_ = false;
  next_status_query_ms_ = 0;
  next_diagnostic_query_ms_ = 0;
  firmware_block_size_ = 512;
}

void Z1Mainboard::receive_from_host(std::string_view bytes) {
  receive_from_host(HostChannel::uart, bytes);
}

void Z1Mainboard::receive_from_host(HostChannel channel, std::string_view bytes) {
  auto& decoder = channel == HostChannel::uart ? uart_decoder_ : tcp_decoder_;
  (channel == HostChannel::uart ? uart_active_ : tcp_active_) = true;
  decoder.append(bytes);
  for (const auto& frame : decoder.take_frames()) handle_host_frame(channel, frame);
}

void Z1Mainboard::receive_from_lpc(std::string_view bytes) {
  lpc_decoder_.append(bytes);
  for (const auto& frame : lpc_decoder_.take_frames()) handle_lpc_frame(frame);
}

std::string Z1Mainboard::take_host_tx() {
  return take_host_tx(HostChannel::uart);
}

std::string Z1Mainboard::take_host_tx(HostChannel channel) {
  auto& output = channel == HostChannel::uart ? uart_tx_ : tcp_tx_;
  auto bytes = std::move(output);
  output.clear();
  return bytes;
}

std::string Z1Mainboard::take_lpc_tx() {
  auto bytes = std::move(lpc_tx_);
  lpc_tx_.clear();
  return bytes;
}

void Z1Mainboard::set_sd_root(std::filesystem::path sd_root) { sd_root_ = std::move(sd_root); }

void Z1Mainboard::tick(std::uint64_t now_ms) {
  if (now_ms >= next_status_query_ms_) {
    send_lpc(makera::PacketType::ControlSingle, "?");
    next_status_query_ms_ = now_ms + 300;
  }
  if (now_ms >= next_diagnostic_query_ms_) {
    send_lpc(makera::PacketType::ControlMulti, "diagnose\n");
    next_diagnostic_query_ms_ = now_ms + 500;
  }
}

void Z1Mainboard::send_host(HostChannel channel, makera::PacketType type, std::string_view payload) {
  auto& output = channel == HostChannel::uart ? uart_tx_ : tcp_tx_;
  output += makera::encode_frame(type, payload);
}

void Z1Mainboard::broadcast_host(makera::PacketType type, std::string_view payload) {
  if (uart_active_) send_host(HostChannel::uart, type, payload);
  if (tcp_active_) send_host(HostChannel::tcp, type, payload);
}

void Z1Mainboard::send_lpc(makera::PacketType type, std::string_view payload) {
  lpc_tx_ += makera::encode_frame(type, payload);
}

std::string Z1Mainboard::status_reply() const {
  auto status = latest_status_;
  const auto close = status.find('>');
  if (close == std::string::npos || status.empty() || status.front() != '<') return {};
  status.resize(close);
  status += "|E:0,0,0,0,0|OTA:0,0>\n";
  return status;
}

void Z1Mainboard::handle_host_frame(HostChannel channel, const makera::Frame& frame) {
  if (frame.type == makera::PacketType::FileStart ||
      (static_cast<std::uint8_t>(frame.type) >= static_cast<std::uint8_t>(makera::PacketType::FileMd5) &&
       static_cast<std::uint8_t>(frame.type) <= static_cast<std::uint8_t>(makera::PacketType::FileRetry))) {
    handle_file_transfer(channel, frame);
    return;
  }
  if (frame.type == makera::PacketType::PlayStatus) {
    if (handle_local_command(channel, frame.payload)) return;
    const std::string payload = play_.running ? play_.display_path + "|" : "|";
    send_host(channel, makera::PacketType::PlayStatus, payload);
    return;
  }
  if (frame.type == makera::PacketType::ControlSingle && !frame.payload.empty() && frame.payload.front() == '?') {
    send_host(channel, makera::PacketType::StatusResponse, status_reply());
    return;
  }
  if (frame.type == makera::PacketType::ControlMulti) {
    if (frame.payload.starts_with("play")) {
      prepare_play(channel, frame.payload);
      send_lpc(frame.type, frame.payload);
      return;
    }
    if (frame.payload.starts_with("diagnose")) {
      send_host(channel, makera::PacketType::DiagnosticResponse, latest_diagnostic_ + "|RSSI:-40");
      return;
    }
    if (frame.payload.starts_with("version")) {
      send_host(channel, makera::PacketType::DiagnosticResponse, "version = " + latest_version_ + ".0.1.13\n");
      return;
    }
    if (handle_local_command(channel, frame.payload)) return;
  }
  lpc_tx_ += makera::encode_frame(frame.type, frame.payload);
}

bool Z1Mainboard::handle_local_command(HostChannel channel, std::string_view command) {
  if (command.starts_with("ftype")) {
    send_host(channel, makera::PacketType::NormalInfo, "ftype = nc\r\n");
    return true;
  }
  if (command.starts_with("ls")) {
    auto argument = decode_argument(command.substr(2));
    bool with_size = false;
    while (argument.starts_with("-")) {
      const auto separator = argument.find(' ');
      const auto option = argument.substr(0, separator);
      with_size = with_size || option.find('s') != std::string::npos;
      argument = separator == std::string::npos ? std::string{} : decode_argument(argument.substr(separator + 1));
    }
    if (argument.empty()) argument = "/sd";
    const auto path = resolve_sd_path(argument);
    std::string reply;
    if (path) {
      std::error_code error;
      for (std::filesystem::directory_iterator it(*path, error), end; !error && it != end; it.increment(error)) {
        auto name = it->path().filename().string();
        if (name.starts_with('.') && name != ".md5" && name != ".lz") continue;
        std::replace(name.begin(), name.end(), ' ', '\x01');
        if (it->is_directory(error)) name += '/';
        if (with_size) {
          const auto size = it->is_regular_file(error) ? it->file_size(error) : 0;
          name += " " + std::to_string(size) + " 19700101000000";
        }
        name += "\r\n";
        if (reply.size() + name.size() > 411 && !reply.empty()) {
          send_host(channel, makera::PacketType::LoadInfo, reply);
          reply.clear();
        }
        reply += name;
      }
    }
    if (!reply.empty()) send_host(channel, makera::PacketType::LoadInfo, reply);
    send_host(channel, makera::PacketType::LoadFinish, "Load directory finished.\r\n");
    return true;
  }
  if (command.starts_with("mkdir")) {
    const auto display = decode_argument(command.substr(5));
    if (display.empty()) return true;
    const auto path = resolve_sd_path(display);
    std::error_code error;
    const bool created = path && std::filesystem::create_directory(*path, error);
    send_host(channel, created ? makera::PacketType::LoadFinish : makera::PacketType::LoadError,
              created ? "ok\r\n" : "could not create directory " + display + "\r\n");
    if (created) send_host(channel, makera::PacketType::NormalInfo, "created directory " + display + "\r\n");
    return true;
  }
  if (command.starts_with("rm")) {
    auto display = decode_argument(command.substr(2));
    if (display.starts_with("-r ") || display.starts_with("-R ")) display = decode_argument(display.substr(3));
    if (display.empty()) return true;
    const auto path = resolve_sd_path(display);
    std::error_code error;
    const auto removed = path ? std::filesystem::remove_all(*path, error) : 0;
    send_host(channel, removed > 0 && !error ? makera::PacketType::LoadFinish : makera::PacketType::LoadError,
              removed > 0 && !error ? "ok\r\n" : "Could not delete " + display + " \r\n");
    return true;
  }
  if (command.starts_with("mv")) {
    const auto arguments = decode_argument(command.substr(2));
    const auto separator = arguments.find(' ');
    if (separator == std::string::npos) return true;
    const auto source_display = arguments.substr(0, separator);
    const auto destination_display = decode_argument(arguments.substr(separator + 1));
    const auto source = resolve_sd_path(source_display);
    const auto destination = resolve_sd_path(destination_display);
    std::error_code error;
    if (source && destination) std::filesystem::rename(*source, *destination, error);
    const bool moved = source && destination && !error;
    send_host(channel, moved ? makera::PacketType::LoadFinish : makera::PacketType::LoadError,
              moved ? "ok\r\n" : "Could not rename " + source_display + " to " + destination_display + "\r\n");
    if (moved) {
      send_host(channel, makera::PacketType::NormalInfo,
                "renamed " + source_display + " to " + destination_display + "\r\n");
    }
    return true;
  }
  if (command.starts_with("md5sum")) {
    const auto display = decode_argument(command.size() >= 7 ? command.substr(7) : std::string_view{});
    if (display.empty()) {
      send_host(channel, makera::PacketType::NormalInfo, "Error: md5sum requires a file path\r\n");
      return true;
    }
    const auto path = resolve_sd_path(display);
    if (!path || !std::filesystem::exists(*path)) {
      send_host(channel, makera::PacketType::NormalInfo, "Error: file not found [" + display + "]\r\n");
    } else if (!std::filesystem::is_regular_file(*path)) {
      send_host(channel, makera::PacketType::NormalInfo, "Error: not a file [" + display + "]\r\n");
    } else {
      send_host(channel, makera::PacketType::NormalInfo, file_md5(*path) + display + "\r\n");
    }
    return true;
  }
  return false;
}

void Z1Mainboard::handle_lpc_frame(const makera::Frame& frame) {
  if (frame.type == makera::PacketType::StatusResponse) {
    if (!frame.payload.empty()) latest_status_ = frame.payload;
    return;
  }
  if (frame.type == makera::PacketType::DiagnosticResponse) {
    if (!frame.payload.empty()) latest_diagnostic_ = frame.payload;
    return;
  }
  if (frame.type == makera::PacketType::FirmwareVersion) {
    if (!frame.payload.empty()) latest_version_ = frame.payload;
    return;
  }
  const auto type = static_cast<std::uint8_t>(frame.type);
  if ((type & 0xf0) == 0xf0) {
    handle_play_frame(frame);
    return;
  }
  if ((type & 0xf0) == 0xd0) {
    handle_transfer(frame, makera::PacketType::ConfigStart, makera::PacketType::ConfigView,
                    makera::PacketType::ConfigData, makera::PacketType::ConfigEnd,
                    makera::PacketType::ConfigCancel, sd_root_ / "config.txt", true);
    return;
  }
  if ((type & 0xf0) == 0xc0) {
    handle_firmware_transfer(frame);
    return;
  }
  if ((type & 0xf0) == 0xe0) {
    handle_transfer(frame, makera::PacketType::FactoryStart, makera::PacketType::FactoryView,
                    makera::PacketType::FactoryData, makera::PacketType::FactoryEnd,
                    makera::PacketType::FactoryCancel, sd_root_ / "factory.ini", false);
    return;
  }
  broadcast_host(frame.type, frame.payload);
}

void Z1Mainboard::handle_firmware_transfer(const makera::Frame& frame) {
  const auto path = sd_root_ / "lpc1768.bin";
  if (frame.type == makera::PacketType::FirmwareEnd || frame.type == makera::PacketType::FirmwareCancel) return;
  if (frame.type == makera::PacketType::FirmwareStart) {
    send_lpc(std::filesystem::is_regular_file(path) ? makera::PacketType::FirmwareStart
                                                   : makera::PacketType::FirmwareCancel,
             {});
    return;
  }
  if (frame.type == makera::PacketType::FirmwareView) {
    if (frame.payload.size() < 6 || !std::filesystem::is_regular_file(path)) {
      send_lpc(makera::PacketType::FirmwareCancel, {});
      return;
    }
    firmware_block_size_ = read_be16(frame.payload, 4);
    if (firmware_block_size_ == 0) {
      send_lpc(makera::PacketType::FirmwareCancel, {});
      return;
    }
    const auto size = std::filesystem::file_size(path);
    std::string reply;
    append_be32(reply, static_cast<std::uint32_t>((size + firmware_block_size_ - 1) / firmware_block_size_));
    append_be16(reply, firmware_block_size_);
    send_lpc(makera::PacketType::FirmwareView, reply);
    return;
  }
  if (frame.type != makera::PacketType::FirmwareData || frame.payload.size() < 4 ||
      !std::filesystem::is_regular_file(path)) {
    send_lpc(makera::PacketType::FirmwareCancel, {});
    return;
  }
  const auto index = read_be32(frame.payload, 0);
  std::ifstream input(path, std::ios::binary);
  input.seekg(static_cast<std::streamoff>((index > 0 ? index - 1 : 0) * firmware_block_size_));
  std::string block(firmware_block_size_, '\0');
  input.read(block.data(), static_cast<std::streamsize>(block.size()));
  block.resize(static_cast<std::size_t>(input.gcount()));
  if (block.empty()) return;
  std::string reply(frame.payload.data(), 4);
  reply += block;
  send_lpc(makera::PacketType::FirmwareData, reply);
}

std::optional<std::filesystem::path> Z1Mainboard::resolve_sd_path(std::string_view encoded_path) const {
  const auto decoded = decode_argument(encoded_path);
  std::filesystem::path relative;
  if (decoded == "/sd" || decoded == "/sd/") return sd_root_;
  if (decoded.starts_with("/sd/")) {
    relative = std::filesystem::path(decoded.substr(4));
  } else if (!decoded.empty() && decoded.front() != '/') {
    relative = std::filesystem::path(decoded);
  } else {
    return std::nullopt;
  }

  std::filesystem::path clean;
  for (const auto& component : relative) {
    if (component == "." || component.empty()) continue;
    if (component == "..") {
      if (!clean.empty()) clean = clean.parent_path();
      continue;
    }
    clean /= component;
  }
  return sd_root_ / clean;
}

void Z1Mainboard::start_file_transfer(HostChannel channel, std::string_view command) {
  const bool upload = command.starts_with("upload");
  const bool download = command.starts_with("download");
  if (!upload && !download) return;

  const auto argument_offset = upload ? std::size_t{6} : std::size_t{8};
  const auto display_path = decode_argument(command.substr(argument_offset));
  const auto path = resolve_sd_path(display_path);
  if (!path || (upload && display_path.empty())) {
    send_host(channel, makera::PacketType::FileCancel, "Error: Invalid filename!\r\n");
    file_transfer_ = {};
    return;
  }

  file_transfer_ = {};
  file_transfer_.owner = channel;
  file_transfer_.path = *path;
  file_transfer_.display_path = display_path.starts_with('/') ? display_path : "/sd/" + display_path;
  file_transfer_.md5_path = md5_sidecar(sd_root_, file_transfer_.display_path);

  if (download) {
    if (!std::filesystem::is_regular_file(file_transfer_.path)) {
      send_host(channel, makera::PacketType::FileCancel,
                "Error: failed to open file [" + file_transfer_.display_path + "]!\r\n");
      file_transfer_ = {};
      return;
    }
    file_transfer_.direction = TransferDirection::download;
    if (file_transfer_.path.filename() == "config.txt") {
      file_transfer_.md5 = file_md5(file_transfer_.path);
    } else if (!file_transfer_.md5_path.empty() && std::filesystem::is_regular_file(file_transfer_.md5_path)) {
      file_transfer_.md5 = read_file(file_transfer_.md5_path).substr(0, 32);
    }
    if (file_transfer_.md5.size() != 32) file_transfer_.md5 = "82df799dde08f3d86839e24cb97e74d4";
    file_transfer_.last_reply_type = makera::PacketType::FileMd5;
    file_transfer_.last_reply_payload = file_transfer_.md5;
    send_host(channel, file_transfer_.last_reply_type, file_transfer_.last_reply_payload);
    return;
  }

  std::error_code error;
  std::filesystem::create_directories(file_transfer_.path.parent_path(), error);
  if (!file_transfer_.md5_path.empty()) std::filesystem::create_directories(file_transfer_.md5_path.parent_path(), error);
  std::ofstream target(file_transfer_.path, std::ios::binary | std::ios::trunc);
  if (!target) {
    send_host(channel, makera::PacketType::FileCancel,
              "Error: failed to open file [" + file_transfer_.display_path + "]!\r\n");
    file_transfer_ = {};
    return;
  }
  file_transfer_.direction = TransferDirection::upload;
  file_transfer_.upload_stage = UploadStage::md5;
  file_transfer_.last_reply_type = makera::PacketType::FileMd5;
  file_transfer_.last_reply_payload.clear();
  send_host(channel, makera::PacketType::FileMd5, {});
}

void Z1Mainboard::handle_file_transfer(HostChannel channel, const makera::Frame& frame) {
  if (frame.type == makera::PacketType::FileStart) {
    start_file_transfer(channel, frame.payload);
    return;
  }
  if (file_transfer_.direction == TransferDirection::none || file_transfer_.owner != channel) return;

  if (frame.type == makera::PacketType::FileCancel) {
    send_host(channel, makera::PacketType::FileCancel,
              file_transfer_.direction == TransferDirection::upload ? "Info: Upload canceled by remote!\r\n"
                                                                    : "Info: canceled by remote!\r\n");
    file_transfer_ = {};
    return;
  }
  if (frame.type == makera::PacketType::FileRetry) {
    send_host(channel, file_transfer_.last_reply_type, file_transfer_.last_reply_payload);
    return;
  }

  if (file_transfer_.direction == TransferDirection::download) {
    if (frame.type == makera::PacketType::FileMd5) {
      file_transfer_.last_reply_type = makera::PacketType::FileMd5;
      file_transfer_.last_reply_payload = file_transfer_.md5;
    } else if (frame.type == makera::PacketType::FileView) {
      const auto size = std::filesystem::file_size(file_transfer_.path);
      file_transfer_.frame_count = static_cast<std::uint32_t>((size + 8191) / 8192);
      file_transfer_.last_reply_payload.clear();
      append_be32(file_transfer_.last_reply_payload, file_transfer_.frame_count);
      append_be16(file_transfer_.last_reply_payload, 8192);
      file_transfer_.last_reply_type = makera::PacketType::FileView;
    } else if (frame.type == makera::PacketType::FileData && frame.payload.size() >= 4) {
      const auto sequence = read_be32(frame.payload, 0);
      std::ifstream input(file_transfer_.path, std::ios::binary);
      input.seekg(static_cast<std::streamoff>((sequence > 0 ? sequence - 1 : 0) * 8192ULL));
      std::string block(8192, '\0');
      input.read(block.data(), static_cast<std::streamsize>(block.size()));
      block.resize(static_cast<std::size_t>(input.gcount()));
      if (block.empty()) {
        send_host(channel, makera::PacketType::FileCancel, "Error: Machine received cmd timeout!\r\n");
        file_transfer_ = {};
        return;
      }
      file_transfer_.sequence = sequence;
      file_transfer_.last_reply_payload.assign(frame.payload.data(), 4);
      file_transfer_.last_reply_payload += block;
      file_transfer_.last_reply_type = makera::PacketType::FileData;
    } else if (frame.type == makera::PacketType::FileEnd) {
      send_host(channel, makera::PacketType::FileEnd, "ok\r\n");
      send_host(channel, makera::PacketType::NormalInfo,
                "Info: download success: " + file_transfer_.display_path + ".\r\n");
      file_transfer_ = {};
      return;
    } else {
      return;
    }
    send_host(channel, file_transfer_.last_reply_type, file_transfer_.last_reply_payload);
    return;
  }

  if (file_transfer_.upload_stage == UploadStage::md5 && frame.type == makera::PacketType::FileMd5 &&
      frame.payload.size() >= 32) {
    file_transfer_.md5.assign(frame.payload.data(), 32);
    if (!file_transfer_.md5_path.empty()) {
      std::ofstream(file_transfer_.md5_path, std::ios::binary | std::ios::trunc) << file_transfer_.md5;
    }
    file_transfer_.upload_stage = UploadStage::layout;
    file_transfer_.last_reply_type = makera::PacketType::FileView;
    file_transfer_.last_reply_payload.clear();
    send_host(channel, makera::PacketType::FileView, {});
    return;
  }
  if (file_transfer_.upload_stage == UploadStage::layout && frame.type == makera::PacketType::FileView &&
      frame.payload.size() >= 4) {
    file_transfer_.frame_count = read_be32(frame.payload, 0);
    file_transfer_.sequence = 1;
    file_transfer_.upload_stage = UploadStage::data;
    file_transfer_.last_reply_payload.clear();
    append_be32(file_transfer_.last_reply_payload, 1);
    file_transfer_.last_reply_type = makera::PacketType::FileData;
    send_host(channel, makera::PacketType::FileData, file_transfer_.last_reply_payload);
    return;
  }
  if (file_transfer_.upload_stage == UploadStage::data && frame.type == makera::PacketType::FileData &&
      frame.payload.size() >= 4 && read_be32(frame.payload, 0) == file_transfer_.sequence) {
    std::ofstream output(file_transfer_.path, std::ios::binary | std::ios::app);
    output.write(frame.payload.data() + 4, static_cast<std::streamsize>(frame.payload.size() - 4));
    if (!output) {
      send_host(channel, makera::PacketType::FileRetry, "Error: File Write error!retry...\r\n");
      return;
    }
    if (file_transfer_.sequence >= file_transfer_.frame_count) {
      send_host(channel, makera::PacketType::FileEnd, "ok\r\n");
      send_host(channel, makera::PacketType::NormalInfo,
                "Info: upload success: " + file_transfer_.display_path + ".\r\n");
      file_transfer_ = {};
      return;
    }
    ++file_transfer_.sequence;
    file_transfer_.last_reply_payload.clear();
    append_be32(file_transfer_.last_reply_payload, file_transfer_.sequence);
    send_host(channel, makera::PacketType::FileData, file_transfer_.last_reply_payload);
  }
}

void Z1Mainboard::prepare_play(HostChannel channel, std::string_view command) {
  auto argument = command.size() > 5 ? command.substr(5) : std::string_view{};
  if (argument.starts_with("play ") && argument.size() > 5) argument.remove_prefix(5);
  const auto display_path = decode_argument(argument);
  const auto path = resolve_sd_path(display_path);
  if (!path || !std::filesystem::is_regular_file(*path)) {
    broadcast_host(makera::PacketType::NormalInfo, "Error:open file failed[P0]");
    return;
  }
  play_ = {};
  play_.owner = channel;
  play_.path = *path;
  play_.display_path = display_path.starts_with('/') ? display_path : "/sd/" + display_path;
  play_.identifier = crc16_ccitt(play_.display_path);
  play_.prepared = true;
}

void Z1Mainboard::handle_play_frame(const makera::Frame& frame) {
  if (frame.type == makera::PacketType::PlayEnd || frame.type == makera::PacketType::PlayCancel) {
    play_.running = false;
    play_.prepared = false;
    return;
  }
  if (frame.type == makera::PacketType::PlayStart) {
    if (!play_.prepared || frame.payload.size() < 2 || read_be16(frame.payload, 0) != play_.identifier) {
      send_lpc(makera::PacketType::PlayCancel, {});
      return;
    }
    std::string reply;
    append_be16(reply, play_.identifier);
    append_be32(reply, static_cast<std::uint32_t>(std::filesystem::file_size(play_.path)));
    send_lpc(makera::PacketType::PlayView, reply);
    play_.running = true;
    return;
  }
  if (frame.type != makera::PacketType::PlayData || frame.payload.size() < 6 || !play_.prepared ||
      read_be16(frame.payload, 0) != play_.identifier) {
    if (frame.type == makera::PacketType::PlayData) send_lpc(makera::PacketType::PlayEnd, {});
    return;
  }

  const auto requested = read_be32(frame.payload, 2);
  const auto maximum_lines = frame.payload.size() >= 8 ? read_be16(frame.payload, 6) : 255;
  const auto lines = play_lines(play_.path);
  if (requested >= lines.size()) {
    send_lpc(makera::PacketType::PlayEnd, {});
    play_.running = false;
    play_.prepared = false;
    return;
  }
  std::string reply(frame.payload.data(), 6);
  const auto count_limit = static_cast<std::size_t>(maximum_lines == 0 ? 255 : maximum_lines);
  std::size_t count = 0;
  for (std::size_t index = requested; index < lines.size() && count < count_limit; ++index, ++count) {
    if (reply.size() + lines[index].size() > 6 + 512) break;
    reply += lines[index];
    if (6 + 512 - reply.size() < 74) break;
  }
  play_.line_index = requested + static_cast<std::uint32_t>(count);
  send_lpc(makera::PacketType::PlayData, reply);
  if (play_.line_index >= lines.size()) {
    send_lpc(makera::PacketType::PlayEnd, {});
    play_.running = false;
    play_.prepared = false;
  }
}

void Z1Mainboard::handle_transfer(const makera::Frame& frame, makera::PacketType start, makera::PacketType view,
                                  makera::PacketType data, makera::PacketType end, makera::PacketType cancel,
                                  const std::filesystem::path& path, bool config_lines) {
  if (frame.type == end) {
    if (!config_lines) {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
    return;
  }
  if (frame.type == cancel) return;
  if (frame.type == start) {
    send_lpc(start, {});
    if (!std::filesystem::is_regular_file(path)) send_lpc(cancel, {});
    return;
  }

  const auto records = config_lines ? config_records(path) : factory_records(path);
  if (frame.type == view) {
    if (frame.payload.size() < 6) {
      send_lpc(cancel, {});
      return;
    }
    std::string reply;
    append_be32(reply, static_cast<std::uint32_t>(records.size()));
    append_be16(reply, config_lines ? 512 : read_be16(frame.payload, 4));
    send_lpc(view, reply);
    return;
  }
  if (frame.type != data || frame.payload.size() < 4) {
    send_lpc(cancel, {});
    return;
  }

  const auto index = read_be32(frame.payload, 0);
  std::string reply(frame.payload.data(), 4);
  if (config_lines) {
    // The motion board numbers config lines from one.  Its next request is the
    // number of accepted lines plus one, so an index of N starts at record
    // N - 1.  Treat zero like one for compatibility with early bootloaders.
    const std::size_t begin = index <= 1 ? 0 : std::min<std::size_t>(index - 1, records.size());
    for (std::size_t i = begin; i < records.size(); ++i) {
      if (reply.size() + records[i].size() > 4 + 512) break;
      reply += records[i];
      if (4 + 512 - reply.size() < 80) break;
    }
    if (reply.size() > 4 && reply.size() < 4 + 512) reply.push_back('\n');
  } else if (index > 0 && index <= records.size() && records[index - 1].size() <= 132) {
    reply += records[index - 1];
  }
  if (reply.size() > 4) send_lpc(data, reply);
}

}  // namespace sim
