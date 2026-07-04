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

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from nicegui import ui

from gui.protocol.model import InteractiveTransportState


def transport_wifi_text(transport: InteractiveTransportState) -> str:
    if not transport.tcp_endpoints:
        return "--"
    return ", ".join(f"{endpoint.host}:{endpoint.port}" for endpoint in transport.tcp_endpoints)


def transport_uart_text(transport: InteractiveTransportState) -> str:
    return transport.uart_path or ("unsupported" if not transport.uart_supported else "--")


@dataclass
class TransportPanelView:
    uart_label: Any
    wifi_label: Any
    machine_model_label: Any

    def update(self, transport: InteractiveTransportState) -> None:
        self.uart_label.text = transport_uart_text(transport)
        self.wifi_label.text = transport_wifi_text(transport)

    def reset(self) -> None:
        self.uart_label.text = "--"
        self.wifi_label.text = "--"


def build_transport_panel(
    *, sd_root: Path, simulator_path: Path, open_sd_root: Callable[[], None]
) -> TransportPanelView:
    with ui.element("div").classes("panel-section"):
        ui.label("Controller Links").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            with ui.element("div").classes("metric"):
                ui.label("UART PTY").classes("metric-name")
                uart_label = ui.label("--").classes("metric-value")
            with ui.element("div").classes("metric"):
                ui.label("WiFi TCP").classes("metric-name")
                wifi_label = ui.label("--").classes("metric-value")
    with ui.element("div").classes("panel-section"):
        ui.label("Startup").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            with ui.element("div").classes("metric"):
                ui.label("Machine shell").classes("metric-name")
                machine_model_label = ui.label("none").classes("badge-off")
        ui.label(f"SD root parent: {sd_root}").classes("section-subtle")
        ui.button("Open SD Card Folder", icon="folder_open", on_click=open_sd_root).props("dense outline")
        ui.label(f"Simulator: {simulator_path}").classes("section-subtle")

    return TransportPanelView(
        uart_label=uart_label,
        wifi_label=wifi_label,
        machine_model_label=machine_model_label,
    )
