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

from collections.abc import Awaitable, Callable, Sequence
from dataclasses import dataclass, field
from typing import Any

from nicegui import ui

PinWatch = tuple[str, int, int]
SwitchWatch = tuple[str, str]


@dataclass
class SignalsTabView:
    front_panel_badges: dict[str, Any] = field(default_factory=dict)
    firmware_switch_controls: dict[str, dict[str, Any]] = field(default_factory=dict)
    laser_badges: dict[str, Any] = field(default_factory=dict)
    laser_power_label: Any | None = None
    pwm_labels: dict[str, Any] = field(default_factory=dict)
    temperature_sensor: Any | None = None
    temperature_celsius: Any | None = None
    temperature_status: Any | None = None


def build_signals_tab(
    *,
    switch_watches: Sequence[SwitchWatch],
    pwm_watches: Sequence[PinWatch],
    set_temperature: Callable[[], Awaitable[None]],
    include_temperature: bool = True,
) -> SignalsTabView:
    view = SignalsTabView()
    with ui.element("div").classes("panel-section"):
        ui.label("Outputs").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            with ui.element("div").classes("metric"):
                ui.label("12V rail").classes("metric-name")
                view.front_panel_badges["v12"] = ui.label("off").classes("badge-off")
            with ui.element("div").classes("metric"):
                ui.label("24V rail").classes("metric-name")
                view.front_panel_badges["v24"] = ui.label("off").classes("badge-off")
            with ui.element("div").classes("metric"):
                view.front_panel_badges["led_name"] = ui.label("Panel LED").classes("metric-name")
                view.front_panel_badges["rgb"] = ui.label("not available").classes("metric-value")
                with ui.element("div").classes("led-strip-row"):
                    view.front_panel_badges["led_strip"] = [ui.element("div").classes("led-segment") for _ in range(5)]
        ui.label("Switches").classes("section-subtle")
        with ui.element("div").classes("switch-table"):
            for heading in ("Output", "Readback", "State"):
                ui.label(heading).classes("table-head")
            for name, label in switch_watches:
                ui.label(label).classes("table-cell")
                readback = ui.label("n/a").classes("table-cell")
                badge = ui.label("off").classes("badge-off")
                view.firmware_switch_controls[name] = {
                    "readback": readback,
                    "badge": badge,
                }
        ui.label("Laser").classes("section-subtle")
        with ui.element("div").classes("metrics-grid"):
            for name, label in (("mode", "Mode"), ("firing", "Firing"), ("testing", "Test")):
                with ui.element("div").classes("metric"):
                    ui.label(label).classes("metric-name")
                    view.laser_badges[name] = ui.label("off").classes("badge-off")
            with ui.element("div").classes("metric"):
                ui.label("Power / scale").classes("metric-name")
                view.laser_power_label = ui.label("n/a").classes("metric-value")
        ui.label("PWM").classes("section-subtle")
        with ui.element("div").classes("pwm-table"):
            for heading in ("Output", "Pin", "Duty / Period"):
                ui.label(heading).classes("table-head")
            for name, port, pin in pwm_watches:
                ui.label(name).classes("table-cell")
                ui.label(f"P{port}.{pin}").classes("table-cell coord-text")
                view.pwm_labels[name] = ui.label("not configured").classes("table-cell")
    if include_temperature:
        with ui.element("div").classes("panel-section"):
            ui.label("Thermistors").classes("section-title")
            with ui.element("div").classes("temperature-drive"):
                view.temperature_sensor = ui.select(
                    {"spindle": "Spindle", "power": "Power"}, value="spindle", label="Sensor"
                ).props("dense outlined")
                view.temperature_celsius = ui.slider(min=0, max=100, value=25.0, step=1.0).props("label-always")
                ui.button("Set Temp", on_click=set_temperature).props("dense color=primary")
            view.temperature_status = ui.label("--").classes("metric-value")
    return view
