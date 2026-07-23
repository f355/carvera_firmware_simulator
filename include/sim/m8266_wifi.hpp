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

#ifndef SIMULATOR_SIM_M8266_WIFI_HPP
#define SIMULATOR_SIM_M8266_WIFI_HPP

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace sim::m8266_wifi {

struct AccessPoint {
  std::string ssid;
  std::string password;
  std::int8_t rssi{-40};
  std::uint8_t authmode{4};
};

class Module {
 public:
  void reset();

  void connect_tcp_client();
  void disconnect_tcp_client();
  bool has_tcp_client() const { return tcp_client_connected_; }

  void receive_tcp(std::string bytes);
  std::string take_tcp_tx();
  std::vector<std::string> take_udp_tx_datagrams();

  bool has_received_data() const;
  std::uint16_t recv_data(std::uint8_t* data, std::uint16_t max_len, std::uint8_t* link_no, std::uint16_t* status);
  std::uint32_t send_tcp(const std::uint8_t* data, std::uint32_t len, std::uint8_t link_no, std::uint16_t* status);
  std::uint16_t send_udp(const std::uint8_t* data, std::uint16_t len, std::uint8_t link_no, std::uint16_t* status);

  void set_connection(std::uint8_t tcp_udp, std::uint16_t local_port, std::uint8_t link_no);
  void delete_connection(std::uint8_t link_no);

  std::uint16_t tcp_server_port() const { return tcp_server_port_; }
  std::uint16_t udp_listen_port() const { return udp_listen_port_; }
  std::uint8_t tcp_link_no() const { return tcp_link_no_; }
  std::uint8_t udp_link_no() const { return udp_link_no_; }

  void set_op_mode(std::uint8_t mode) { op_mode_ = mode; }
  std::uint8_t op_mode() const { return op_mode_; }

  void set_ap_param(std::uint8_t param_type, const std::uint8_t* data, std::uint8_t len);
  std::string ap_param(std::uint8_t param_type) const;
  std::string sta_param(std::uint8_t param_type) const;

  void connect_sta(std::string ssid, std::string password);
  void disconnect_sta();
  std::uint8_t sta_connection_status() const { return sta_connected_ ? 5 : 0; }
  std::string sta_ssid() const { return sta_ssid_; }
  std::int8_t sta_rssi() const;

  const std::vector<AccessPoint>& scanned_access_points() const { return scanned_access_points_; }

 private:
  std::uint8_t tcp_link_no_{0};
  std::uint8_t udp_link_no_{1};
  std::uint16_t tcp_server_port_{0};
  std::uint16_t udp_listen_port_{0};
  bool tcp_client_connected_{false};
  std::uint8_t op_mode_{3};
  bool sta_connected_{true};
  std::string sta_ssid_{"CarveraDev"};
  std::string sta_password_;
  std::string sta_ip_{"127.0.0.1"};
  std::string sta_netmask_{"255.0.0.0"};
  std::string sta_gateway_{"127.0.0.1"};
  std::string sta_hostname_{"CARVERA_SIM"};
  std::string ap_ssid_{"CARVERA_SIM"};
  std::string ap_password_;
  std::uint8_t ap_channel_{6};
  std::uint8_t ap_authmode_{0};
  std::uint8_t ap_phy_mode_{3};
  std::string ap_ip_{"192.168.4.10"};
  std::string ap_netmask_{"255.255.255.0"};
  std::string ap_gateway_{"192.168.4.10"};
  std::deque<std::uint8_t> tcp_rx_;
  std::string tcp_tx_;
  std::deque<std::string> udp_tx_;
  std::vector<AccessPoint> scanned_access_points_{{"CarveraDev", "", -42, 0}, {"Workshop", "password", -61, 4}};
};

Module& active();

}  // namespace sim::m8266_wifi

#endif
