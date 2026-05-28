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
DEFAULT_STOCK_BOX = {
    "min_x": -30.0,
    "min_y": -30.0,
    "min_z": -20.0,
    "max_x": 30.0,
    "max_y": 30.0,
    "max_z": -1.0,
}


@dataclass
class GpioTabView:
    box_controls: dict[str, dict[str, Any]] = field(default_factory=dict)
    pin_badges: dict[str, Any] = field(default_factory=dict)


def build_gpio_tab(
    *,
    pin_watches: Sequence[PinWatch],
    apply_stock: Callable[[], Awaitable[None]],
    include_stock: bool = True,
    include_pin_watch: bool = True,
) -> GpioTabView:
    view = GpioTabView()
    if include_stock:
        with ui.element("div").classes("panel-section"):
            ui.label("Stock Geometry").classes("section-title")
            ui.label("Stock box").classes("section-subtle")
            enabled = ui.switch("Enabled", value=False).props("dense")
            with ui.element("div").classes("box-grid"):
                min_x = ui.number("min X", value=DEFAULT_STOCK_BOX["min_x"], step=1.0).props("dense outlined")
                min_y = ui.number("min Y", value=DEFAULT_STOCK_BOX["min_y"], step=1.0).props("dense outlined")
                min_z = ui.number("min Z", value=DEFAULT_STOCK_BOX["min_z"], step=1.0).props("dense outlined")
                max_x = ui.number("max X", value=DEFAULT_STOCK_BOX["max_x"], step=1.0).props("dense outlined")
                max_y = ui.number("max Y", value=DEFAULT_STOCK_BOX["max_y"], step=1.0).props("dense outlined")
                max_z = ui.number("max Z", value=DEFAULT_STOCK_BOX["max_z"], step=1.0).props("dense outlined")
            view.box_controls["stock"] = {
                "enabled": enabled,
                "min_x": min_x,
                "min_y": min_y,
                "min_z": min_z,
                "max_x": max_x,
                "max_y": max_y,
                "max_z": max_z,
            }
            with ui.element("div").classes("button-row"):
                ui.button("Apply Stock", on_click=apply_stock).props("dense color=primary")
    if include_pin_watch:
        with ui.element("div").classes("panel-section"):
            ui.label("GPIO Watch").classes("section-title")
            with ui.element("div").classes("pin-table"):
                for heading in ("Signal", "Pin", "Level"):
                    ui.label(heading).classes("table-head")
                for name, port, pin in pin_watches:
                    ui.label(name).classes("table-cell")
                    ui.label(f"P{port}.{pin}").classes("table-cell coord-text")
                    view.pin_badges[name] = ui.label("low").classes("badge-off")
    return view
