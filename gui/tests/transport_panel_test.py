# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

from __future__ import annotations

from gui.protocol.model import InteractiveTransportState, TransportEndpoint
from gui.views.transport_panel import transport_uart_text, transport_wifi_text


def test_transport_panel_test() -> None:
    unsupported = InteractiveTransportState(uart_path="", uart_supported=False, tcp_endpoints=())
    if transport_uart_text(unsupported) != "unsupported":
        raise SystemExit("unsupported UART should be explicit")

    pending = InteractiveTransportState(uart_path="", uart_supported=True, tcp_endpoints=())
    if transport_uart_text(pending) != "--":
        raise SystemExit("supported UART without path should use placeholder")

    active = InteractiveTransportState(uart_path="/dev/ttys123", uart_supported=True, tcp_endpoints=())
    if transport_uart_text(active) != "/dev/ttys123":
        raise SystemExit("active UART should show the PTY path")

    no_tcp = InteractiveTransportState(uart_path="", uart_supported=True, tcp_endpoints=())
    if transport_wifi_text(no_tcp) != "--":
        raise SystemExit("missing TCP endpoints should use placeholder")

    tcp = InteractiveTransportState(
        uart_path="",
        uart_supported=True,
        tcp_endpoints=(
            TransportEndpoint(host="127.0.0.1", port=2222),
            TransportEndpoint(host="127.0.0.1", port=2223),
        ),
    )
    if transport_wifi_text(tcp) != "127.0.0.1:2222, 127.0.0.1:2223":
        raise SystemExit("TCP endpoints should be formatted for display")
