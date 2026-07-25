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

#include "sim/m8266_wifi.hpp"

#include <algorithm>
#include <cstring>

#include "M8266WIFIDrv.h"
#include "M8266HostIf.h"
#include "sim/simulator_context.hpp"
#include "compat/active_context.hpp"

namespace sim::m8266_wifi {
namespace {

constexpr std::uint16_t status_ok = 0;
constexpr std::uint16_t status_no_data = 0x22;
constexpr std::uint16_t status_no_client = 0x18;

void set_status(std::uint16_t* status, std::uint16_t value = status_ok) {
  if (status != nullptr) {
    *status = value;
  }
}

void copy_string_param(const std::string& value, std::uint8_t* param, std::uint8_t* param_len) {
  const auto len = std::min<std::size_t>(value.size(), 63);
  if (param != nullptr) {
    std::memcpy(param, value.data(), len);
    param[len] = '\0';
  }
  if (param_len != nullptr) {
    *param_len = static_cast<std::uint8_t>(len);
  }
}

}  // namespace

void Module::reset() {
  tcp_link_no_ = 0;
  udp_link_no_ = 1;
  tcp_server_port_ = 0;
  udp_listen_port_ = 0;
  tcp_client_connected_ = false;
  op_mode_ = 3;
  sta_connected_ = true;
  tcp_rx_.clear();
  tcp_tx_.clear();
  udp_tx_.clear();
}

void Module::connect_tcp_client() { tcp_client_connected_ = true; }

void Module::disconnect_tcp_client() { tcp_client_connected_ = false; }

void Module::receive_tcp(std::string bytes) {
  for (const auto byte : bytes) {
    tcp_rx_.push_back(static_cast<std::uint8_t>(byte));
  }
}

std::string Module::take_tcp_tx() {
  auto out = std::move(tcp_tx_);
  tcp_tx_.clear();
  return out;
}

std::vector<std::string> Module::take_udp_tx_datagrams() {
  std::vector<std::string> out;
  out.reserve(udp_tx_.size());
  while (!udp_tx_.empty()) {
    out.push_back(std::move(udp_tx_.front()));
    udp_tx_.pop_front();
  }
  return out;
}

bool Module::has_received_data() const { return !tcp_rx_.empty(); }

std::uint16_t Module::recv_data(std::uint8_t* data, std::uint16_t max_len, std::uint8_t* link_no,
                                std::uint16_t* status) {
  if (tcp_rx_.empty()) {
    if (link_no != nullptr) {
      *link_no = tcp_link_no_;
    }
    set_status(status, status_no_data);
    return 0;
  }

  const auto count = std::min<std::size_t>(max_len, tcp_rx_.size());
  for (std::size_t i = 0; i < count; ++i) {
    data[i] = tcp_rx_.front();
    tcp_rx_.pop_front();
  }
  if (link_no != nullptr) {
    *link_no = tcp_link_no_;
  }
  set_status(status);
  return static_cast<std::uint16_t>(count);
}

std::uint32_t Module::send_tcp(const std::uint8_t* data, std::uint32_t len, std::uint8_t link_no,
                               std::uint16_t* status) {
  if (link_no != tcp_link_no_ || !tcp_client_connected_) {
    set_status(status, status_no_client);
    return 0;
  }
  tcp_tx_.append(reinterpret_cast<const char*>(data), len);
  set_status(status);
  return len;
}

std::uint16_t Module::send_udp(const std::uint8_t* data, std::uint16_t len, std::uint8_t link_no,
                               std::uint16_t* status) {
  if (link_no != udp_link_no_) {
    set_status(status, status_no_data);
    return 0;
  }
  udp_tx_.emplace_back(reinterpret_cast<const char*>(data), len);
  set_status(status);
  return len;
}

void Module::set_connection(std::uint8_t tcp_udp, std::uint16_t local_port, std::uint8_t link_no) {
  if (tcp_udp == 2) {
    tcp_link_no_ = link_no;
    tcp_server_port_ = local_port;
  } else if (tcp_udp == 0) {
    udp_link_no_ = link_no;
    udp_listen_port_ = local_port;
  }
}

void Module::delete_connection(std::uint8_t link_no) {
  if (link_no == tcp_link_no_) {
    tcp_client_connected_ = false;
    tcp_server_port_ = 0;
    tcp_rx_.clear();
    tcp_tx_.clear();
  } else if (link_no == udp_link_no_) {
    udp_listen_port_ = 0;
    udp_tx_.clear();
  }
}

void Module::set_ap_param(std::uint8_t param_type, const std::uint8_t* data, std::uint8_t len) {
  const std::string value(reinterpret_cast<const char*>(data), len);
  switch (param_type) {
    case AP_PARAM_TYPE_SSID:
      ap_ssid_ = value;
      break;
    case AP_PARAM_TYPE_PASSWORD:
      ap_password_ = value;
      break;
    case AP_PARAM_TYPE_CHANNEL:
      ap_channel_ = len == 0 ? 0 : data[0];
      break;
    case AP_PARAM_TYPE_AUTHMODE:
      ap_authmode_ = len == 0 ? 0 : data[0];
      break;
    default:
      break;
  }
}

std::string Module::ap_param(std::uint8_t param_type) const {
  switch (param_type) {
    case AP_PARAM_TYPE_SSID:
      return ap_ssid_;
    case AP_PARAM_TYPE_PASSWORD:
      return ap_password_;
    case AP_PARAM_TYPE_IP_ADDR:
      return ap_ip_;
    case AP_PARAM_TYPE_GATEWAY_ADDR:
      return ap_gateway_;
    case AP_PARAM_TYPE_NETMASK_ADDR:
      return ap_netmask_;
    case AP_PARAM_TYPE_CHANNEL:
      return std::string(1, static_cast<char>(ap_channel_));
    case AP_PARAM_TYPE_AUTHMODE:
      return std::string(1, static_cast<char>(ap_authmode_));
    case AP_PARAM_TYPE_PHY_MODE:
      return std::string(1, static_cast<char>(ap_phy_mode_));
    default:
      return {};
  }
}

std::string Module::sta_param(std::uint8_t param_type) const {
  switch (param_type) {
    case STA_PARAM_TYPE_SSID:
      return sta_ssid_;
    case STA_PARAM_TYPE_PASSWORD:
      return sta_password_;
    case STA_PARAM_TYPE_HOSTNAME:
      return sta_hostname_;
    case STA_PARAM_TYPE_IP_ADDR:
      return sta_ip_;
    case STA_PARAM_TYPE_GATEWAY_ADDR:
      return sta_gateway_;
    case STA_PARAM_TYPE_NETMASK_ADDR:
      return sta_netmask_;
    case STA_PARAM_TYPE_CHANNEL:
      return std::string(1, '\x06');
    case STA_PARAM_TYPE_MAC:
      return std::string("\x02\x00\x00\x00\x00\x01", 6);
    default:
      return {};
  }
}

void Module::connect_sta(std::string ssid, std::string password) {
  sta_ssid_ = std::move(ssid);
  sta_password_ = std::move(password);
  sta_connected_ = true;
}

void Module::disconnect_sta() { sta_connected_ = false; }

std::int8_t Module::sta_rssi() const {
  if (!sta_connected_) {
    return 31;  // driver convention for "error / not connected"
  }
  for (const auto& ap : scanned_access_points_) {
    if (ap.ssid == sta_ssid_) {
      return ap.rssi;
    }
  }
  return -40;
}

Module& active() { return compat::active_context().m8266_wifi(); }

}  // namespace sim::m8266_wifi

void M8266HostIf_Init(void) {}
void M8266HostIf_GPIO_CS_RESET_Init(void) {}
void M8266HostIf_SPI_SetSpeed(u32) {}
void M8266HostIf_usart_txd_data(char*, u16) {}
void M8266HostIf_usart_txd_string(char*) {}
void M8266HostIf_USART_Reset_RX_BUFFER(void) {}
u16 M8266HostIf_USART_RX_BUFFER_Data_count(void) { return 0; }
u16 M8266HostIf_USART_get_data(u8*, u16) { return 0; }
void M8266HostIf_Set_nRESET_Pin(u8) {}
void M8266HostIf_Set_SPI_nCS_Pin(u8) {}
void M8266HostIf_delay_us(u8) {}
u8 M8266HostIf_SPI_ReadWriteByte(u8 byte) { return byte; }

extern "C" {

u8 M8266HostIf_SPI_Select(uint32_t, uint32_t, u16* status) {
  sim::m8266_wifi::active().reset();
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Interface_Communication_OK(u8* byte) {
  if (byte != nullptr) {
    *byte = 0x41;
  }
  return 1;
}

u32 M8266WIFI_SPI_Interface_Communication_Stress_Test(u32 max_times) { return max_times; }

u8 M8266WIFI_SPI_Set_Tx_Max_Power(u8, u16* status) {
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Setup_Connection(u8 tcp_udp, u16 local_port, char*, u16, u8 link_no, u8, u16* status) {
  sim::m8266_wifi::active().set_connection(tcp_udp, local_port, link_no);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Delete_Connection(u8 link_no, u16* status) {
  sim::m8266_wifi::active().delete_connection(link_no);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Disconnect_Connection(u8 link_no, u16* status) {
  return M8266WIFI_SPI_Delete_Connection(link_no, status);
}

u8 M8266WIFI_SPI_Set_TcpServer_Auto_Discon_Timeout(u8, u16, u16* status) {
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_List_Clients_On_A_TCP_Server(u8, u8* clients, ClientInfo[], u16* status) {
  if (clients != nullptr) {
    *clients = sim::m8266_wifi::active().has_tcp_client() ? 1 : 0;
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Get_STA_Connection_Status(u8* connection_status, u16* status) {
  if (connection_status != nullptr) {
    *connection_status = sim::m8266_wifi::active().sta_connection_status();
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Query_AP_Param(AP_PARAM_TYPE param_type, u8* param, u8* param_len, u16* status) {
  sim::m8266_wifi::copy_string_param(sim::m8266_wifi::active().ap_param(param_type), param, param_len);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Query_STA_Param(STA_PARAM_TYPE param_type, u8* param, u8* param_len, u16* status) {
  sim::m8266_wifi::copy_string_param(sim::m8266_wifi::active().sta_param(param_type), param, param_len);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Config_AP_Param(AP_PARAM_TYPE param_type, u8* param, u8 param_len, u8, u16* status) {
  sim::m8266_wifi::active().set_ap_param(param_type, param, param_len);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Get_Opmode(u8* op_mode, u16* status) {
  if (op_mode != nullptr) {
    *op_mode = sim::m8266_wifi::active().op_mode();
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Set_Opmode(u8 op_mode, u8, u16* status) {
  sim::m8266_wifi::active().set_op_mode(op_mode);
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_STA_Connect_Ap(u8 ssid[32], u8 password[64], u8, u8, u16* status) {
  sim::m8266_wifi::active().connect_sta(reinterpret_cast<const char*>(ssid), reinterpret_cast<const char*>(password));
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_STA_DisConnect_Ap(u16* status) {
  sim::m8266_wifi::active().disconnect_sta();
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Get_STA_IP_Addr(char* sta_ip, u16* status) {
  const auto ip = sim::m8266_wifi::active().sta_param(STA_PARAM_TYPE_IP_ADDR);
  std::strcpy(sta_ip, ip.c_str());
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_STA_Query_Current_SSID_And_RSSI(u8 ssid[32], s8* rssi, u16* status) {
  auto& wifi = sim::m8266_wifi::active();
  if (!wifi.sta_connection_status()) {
    if (rssi != nullptr) {
      *rssi = 31;
    }
    sim::m8266_wifi::set_status(status, 0x0100);
    return 0;
  }

  const auto current_ssid = wifi.sta_ssid();
  if (ssid != nullptr) {
    std::memset(ssid, 0, 32);
    std::memcpy(ssid, current_ssid.data(), std::min<std::size_t>(current_ssid.size(), 31));
  }
  if (rssi != nullptr) {
    *rssi = wifi.sta_rssi();
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_STA_ScanSignals(struct ScannedSigs[], u8, u8, u8, u8, u32, u32, u8, u16* status) {
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_STA_Fetch_Last_Scanned_Signals(struct ScannedSigs scanned_signals[], u8 max_signals, u16* status) {
  const auto& aps = sim::m8266_wifi::active().scanned_access_points();
  const auto count = std::min<std::size_t>(max_signals, aps.size());
  for (std::size_t i = 0; i < count; ++i) {
    std::strncpy(scanned_signals[i].ssid, aps[i].ssid.c_str(), sizeof(scanned_signals[i].ssid) - 1);
    scanned_signals[i].ssid[sizeof(scanned_signals[i].ssid) - 1] = '\0';
    scanned_signals[i].rssi = aps[i].rssi;
    scanned_signals[i].authmode = aps[i].authmode;
  }
  sim::m8266_wifi::set_status(status);
  return static_cast<u8>(count);
}

u8 M8266WIFI_SPI_Has_DataReceived(void) { return sim::m8266_wifi::active().has_received_data() ? 1 : 0; }

u16 M8266WIFI_SPI_RecvData(u8 Data[], u16 max_len, uint16_t max_wait_in_ms, u8* link_no, u16* status) {
  const auto received = sim::m8266_wifi::active().recv_data(Data, max_len, link_no, status);
  auto& clock = sim::compat::active_context().clock();
  if (received == 0 && max_wait_in_ms != 0 && !clock.is_realtime()) {
    clock.advance_us(static_cast<std::uint64_t>(max_wait_in_ms) * 1'000);
  }
  return received;
}

u16 M8266WIFI_SPI_RecvData_ex(u8 Data[], u16 max_len, uint16_t max_wait_in_ms, u8* link_no, u8[4], u16*, u16* status) {
  return M8266WIFI_SPI_RecvData(Data, max_len, max_wait_in_ms, link_no, status);
}

u16 M8266WIFI_SPI_Send_Data(u8 Data[], u16 Data_len, u8 link_no, u16* status) {
  return static_cast<u16>(sim::m8266_wifi::active().send_tcp(Data, Data_len, link_no, status));
}

u32 M8266WIFI_SPI_Send_BlockData(u8 Data[], u32 Data_len, u16, u8 link_no, char*, u16, u16* status) {
  return sim::m8266_wifi::active().send_tcp(Data, Data_len, link_no, status);
}

u16 M8266WIFI_SPI_Send_Udp_Data(u8 Data[], u16 Data_len, u8 link_no, char*, u16, u16* status) {
  return sim::m8266_wifi::active().send_udp(Data, Data_len, link_no, status);
}

u8 M8266WIFI_SPI_Query_Connection(u8 link_no, u8* connection_type, u8* connection_state, u16* local_port, u8*, u16*,
                                  u16* status) {
  auto& wifi = sim::m8266_wifi::active();
  if (connection_type != nullptr) {
    *connection_type = link_no == wifi.tcp_link_no() ? 2 : 0;
  }
  if (connection_state != nullptr) {
    *connection_state = wifi.has_tcp_client() ? 1 : 0;
  }
  if (local_port != nullptr) {
    *local_port = link_no == wifi.tcp_link_no() ? wifi.tcp_server_port() : wifi.udp_listen_port();
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

u8 M8266WIFI_SPI_Get_Module_Info(u32* module_id, u8* flash_size, char* fw_ver, u16* status) {
  if (module_id != nullptr) {
    *module_id = 0x82660001;
  }
  if (flash_size != nullptr) {
    *flash_size = 4;
  }
  if (fw_ver != nullptr) {
    std::strcpy(fw_ver, "sim-m8266");
  }
  sim::m8266_wifi::set_status(status);
  return 1;
}

char* M8266WIFI_SPI_Get_Driver_Info(char* drv_info) {
  std::strcpy(drv_info, "sim-m8266-driver");
  return drv_info;
}
}
