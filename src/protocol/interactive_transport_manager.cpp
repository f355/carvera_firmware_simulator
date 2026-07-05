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

#include "sim/interactive_transport_manager.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>
#include <utility>

#include "sim/delay_hooks.hpp"
#include "sim/logging.hpp"
#include "sim/machine_simulator.hpp"
#include "sim/runtime_io.hpp"
#include "sim/runtime_pump.hpp"

namespace {

constexpr std::size_t kBaseFreeRunningTimerTicks = 1000;
constexpr std::size_t kBaseFreeRunningMainLoops = 4;
constexpr std::size_t kMaxFreeRunningBatches = 100;

sim::InteractiveTransportStartResult start_error(std::string message) {
  sim::InteractiveTransportStartResult result;
  result.ok = false;
  result.error = std::move(message);
  return result;
}

std::string localhost_discovery_payload(std::string_view firmware_payload, std::uint16_t tcp_port) {
  const auto first_comma = firmware_payload.find(',');
  if (first_comma == std::string_view::npos) {
    return std::string(firmware_payload);
  }
  const auto second_comma = firmware_payload.find(',', first_comma + 1);
  if (second_comma == std::string_view::npos) {
    return std::string(firmware_payload);
  }
  const auto third_comma = firmware_payload.find(',', second_comma + 1);
  if (third_comma == std::string_view::npos) {
    return std::string(firmware_payload);
  }

  std::string payload(firmware_payload.substr(0, first_comma));
  payload += ",127.0.0.1,";
  payload += std::to_string(tcp_port);
  payload += firmware_payload.substr(third_comma);
  return payload;
}

std::size_t free_running_batches(double realtime_speed) {
  if (!std::isfinite(realtime_speed) || realtime_speed <= 1.0) {
    return 1;
  }
  const auto scaled = static_cast<std::size_t>(std::ceil(realtime_speed));
  return std::clamp(scaled, std::size_t{1}, kMaxFreeRunningBatches);
}

}  // namespace

namespace sim {

InteractiveTransportManager::InteractiveTransportManager(RuntimeIo& io, RuntimePump& runner, MachineSimulator& machine,
                                                         UploadingQuery uploading)
    : runtime_io_(io), runner_(runner), machine_(machine), uploading_(std::move(uploading)) {}

InteractiveTransportStartResult InteractiveTransportManager::start(
    const carvera::sim::v1::StartInteractiveTransport& command) {
  traffic_logging_ = command.log_traffic();

  if (command.enable_uart() && !uart_) {
    auto uart = std::make_unique<VirtualComPort>(runtime_io_);
    if (uart->supported() && !uart->start()) {
      return start_error("failed to start virtual COM port");
    }
    if (uart->supported()) {
      uart_ = std::move(uart);
    }
  }

  if (tcp_bridges_.empty()) {
    for (const auto requested_port : command.tcp_ports()) {
      if (requested_port > 0xffff) {
        return start_error("TCP port must fit in uint16");
      }
      auto bridge = std::make_unique<LocalhostTcpBridge>(runtime_io_, uploading_);
      if (!bridge->start(static_cast<std::uint16_t>(requested_port))) {
        return start_error("failed to start TCP bridge");
      }
      tcp_bridges_.push_back(std::move(bridge));
    }
    if (!tcp_bridges_.empty()) {
      discovery_beacon_.start();
    }
  }

  InteractiveTransportStartResult result;
  fill(result.transport);
  std::ostringstream message;
  message << "started";
  if (result.transport.uart_path().empty()) {
    message << " uart=none";
  } else {
    message << " uart=" << result.transport.uart_path();
  }
  for (const auto& endpoint : result.transport.tcp_endpoints()) {
    message << " tcp=" << endpoint.host() << ':' << endpoint.port();
  }
  logging::event("transport", message.str());
  return result;
}

void InteractiveTransportManager::stop() {
  if (active()) {
    logging::event("transport", "stopped");
  }
  uart_.reset();
  tcp_bridges_.clear();
  discovery_beacon_.stop();
  traffic_logging_ = false;
}

bool InteractiveTransportManager::active() const { return uart_ != nullptr || !tcp_bridges_.empty(); }

void InteractiveTransportManager::pump() {
  if (!active()) {
    return;
  }

  auto pump_transport_io = [this]() {
    if (uart_) {
      log_traffic("uart", "rx", uart_->poll_input());
    }
    for (auto& bridge : tcp_bridges_) {
      log_traffic("wifi", "rx", bridge->poll_input());
    }

    const auto uart_output = runtime_io_.read_serial();
    if (uart_) {
      log_traffic("uart", "tx", uart_output);
      uart_->write_output(uart_output);
    }
    const auto wifi_output = runtime_io_.read_wifi_tcp();
    for (auto& bridge : tcp_bridges_) {
      log_traffic("wifi", "tx", wifi_output);
      bridge->write_output(wifi_output);
    }

    if (auxiliary_pump_) {
      auxiliary_pump_();
    }
  };

  pump_transport_io();
  relay_wifi_discovery();
  discovery_beacon_.poll();

  {
    delay_hooks::ScopedCallback delay_io_pump(pump_transport_io);
    const auto batches = free_running_batches(machine_.realtime_speed());
    for (std::size_t batch = 0; batch < batches; ++batch) {
      runner_.pump_free_running(kBaseFreeRunningMainLoops, kBaseFreeRunningTimerTicks);
      pump_transport_io();
      relay_wifi_discovery();
      discovery_beacon_.poll();
    }
  }

  pump_transport_io();
  relay_wifi_discovery();
  discovery_beacon_.poll();
}

void InteractiveTransportManager::relay_wifi_discovery() {
  if (tcp_bridges_.empty()) {
    return;
  }

  const auto tcp_port = tcp_bridges_.front()->port();
  for (const auto& firmware_payload : runtime_io_.take_wifi_udp_datagrams()) {
    discovery_beacon_.publish(localhost_discovery_payload(firmware_payload, tcp_port));
  }
}

void InteractiveTransportManager::fill(carvera::sim::v1::InteractiveTransport& transport) const {
  VirtualComPort support_probe(runtime_io_);
  transport.set_uart_supported(support_probe.supported());
  if (uart_) {
    transport.set_uart_path(uart_->device_path());
  }
  for (const auto& bridge : tcp_bridges_) {
    auto* endpoint = transport.add_tcp_endpoints();
    endpoint->set_host("0.0.0.0");
    endpoint->set_port(bridge->port());
  }
}

void InteractiveTransportManager::set_auxiliary_pump(AuxiliaryPump pump) { auxiliary_pump_ = std::move(pump); }

void InteractiveTransportManager::log_traffic(const char* channel, const char* direction,
                                              const std::string& bytes) const {
  if (!traffic_logging_ || bytes.empty()) {
    return;
  }
  logging::traffic(channel, direction, bytes);
}

}  // namespace sim
