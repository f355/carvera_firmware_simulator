/*
 * This file is part of the Carvera Firmware Simulator.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SIMULATOR_SIM_Z1_MAINBOARD_HPP
#define SIMULATOR_SIM_Z1_MAINBOARD_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "sim/makera_protocol.hpp"

namespace sim {

// Host-native model of the Z1 ESP32 communications mainboard. It deliberately
// models the documented wire contract, not ESP32 implementation details.
class Z1Mainboard {
 public:
  enum class HostChannel { uart, tcp };

  explicit Z1Mainboard(std::filesystem::path sd_root);

  void receive_from_host(std::string_view bytes);
  void receive_from_host(HostChannel channel, std::string_view bytes);
  void receive_from_lpc(std::string_view bytes);
  std::string take_host_tx();
  std::string take_host_tx(HostChannel channel);
  std::string take_lpc_tx();
  void set_sd_root(std::filesystem::path sd_root);
  void tick(std::uint64_t now_ms);
  void reset();

 private:
  void handle_host_frame(HostChannel channel, const makera::Frame& frame);
  void handle_lpc_frame(const makera::Frame& frame);
  void handle_file_transfer(HostChannel channel, const makera::Frame& frame);
  bool handle_local_command(HostChannel channel, std::string_view command);
  void start_file_transfer(HostChannel channel, std::string_view command);
  void handle_play_frame(const makera::Frame& frame);
  void handle_firmware_transfer(const makera::Frame& frame);
  void prepare_play(HostChannel channel, std::string_view command);
  void handle_transfer(const makera::Frame& frame, makera::PacketType start, makera::PacketType view,
                       makera::PacketType data, makera::PacketType end, makera::PacketType cancel,
                       const std::filesystem::path& path, bool config_lines);
  void send_host(HostChannel channel, makera::PacketType type, std::string_view payload);
  void broadcast_host(makera::PacketType type, std::string_view payload);
  void send_lpc(makera::PacketType type, std::string_view payload);
  std::string status_reply() const;
  std::optional<std::filesystem::path> resolve_sd_path(std::string_view path) const;

  enum class TransferDirection { none, upload, download };
  enum class UploadStage { md5, layout, data };
  struct FileTransfer {
    TransferDirection direction{TransferDirection::none};
    HostChannel owner{HostChannel::uart};
    UploadStage upload_stage{UploadStage::md5};
    std::filesystem::path path;
    std::filesystem::path md5_path;
    std::string display_path;
    std::string md5;
    std::uint32_t frame_count{0};
    std::uint32_t sequence{0};
    makera::PacketType last_reply_type{makera::PacketType::FileCancel};
    std::string last_reply_payload;
  };

  struct StreamedPlay {
    HostChannel owner{HostChannel::uart};
    std::filesystem::path path;
    std::string display_path;
    std::uint16_t identifier{0};
    std::uint32_t line_index{0};
    bool prepared{false};
    bool running{false};
  };

  std::filesystem::path sd_root_;
  makera::FrameDecoder uart_decoder_;
  makera::FrameDecoder tcp_decoder_;
  makera::FrameDecoder lpc_decoder_;
  std::string uart_tx_;
  std::string tcp_tx_;
  std::string lpc_tx_;
  std::string latest_status_;
  std::string latest_diagnostic_;
  std::string latest_version_;
  FileTransfer file_transfer_;
  StreamedPlay play_;
  bool uart_active_{false};
  bool tcp_active_{false};
  std::uint64_t next_status_query_ms_{0};
  std::uint64_t next_diagnostic_query_ms_{0};
  std::uint16_t firmware_block_size_{512};
};

}  // namespace sim

#endif
