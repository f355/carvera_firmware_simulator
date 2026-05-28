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

from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from typing import Any

from nicegui import ui

from .tool_table import default_tool_map, stickout_from_physical_length


@dataclass
class AtcTabView:
    available_badge: Any
    active_tool_label: Any
    target_tool_label: Any
    tlo_label: Any
    held_length_label: Any
    tool_rows: dict[int, dict[str, Any]] = field(default_factory=dict)


def build_atc_tab(
    *,
    apply_rack: Callable[[], Awaitable[None]],
    load_pocket: Callable[[int], Awaitable[None]],
    unload_spindle: Callable[[], Awaitable[None]],
    load_defaults: Callable[[], None],
    clear_loaded: Callable[[], None],
) -> AtcTabView:
    view = build_atc_status_panel()
    view.tool_rows.update(
        build_tool_table_panel(
            apply_rack=apply_rack,
            load_pocket=load_pocket,
            unload_spindle=unload_spindle,
            load_defaults=load_defaults,
            clear_loaded=clear_loaded,
        )
    )
    return view


def build_atc_status_panel() -> AtcTabView:
    with ui.element("div").classes("panel-section"):
        ui.label("ATC State").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            with ui.element("div").classes("metric"):
                ui.label("Mode").classes("metric-name")
                available_badge = ui.label("manual").classes("badge-off")
            with ui.element("div").classes("metric"):
                ui.label("Active tool").classes("metric-name")
                active_tool_label = ui.label("-1").classes("metric-value")
            with ui.element("div").classes("metric"):
                ui.label("Target tool").classes("metric-name")
                target_tool_label = ui.label("-1").classes("metric-value")
            with ui.element("div").classes("metric"):
                ui.label("TLO").classes("metric-name")
                tlo_label = ui.label("0.000 mm").classes("metric-value")
            with ui.element("div").classes("metric"):
                ui.label("Held stickout").classes("metric-name")
                held_length_label = ui.label("0.0 mm").classes("metric-value")

    return AtcTabView(
        available_badge=available_badge,
        active_tool_label=active_tool_label,
        target_tool_label=target_tool_label,
        tlo_label=tlo_label,
        held_length_label=held_length_label,
    )


def build_tool_table_panel(
    *,
    apply_rack: Callable[[], Awaitable[None]],
    load_pocket: Callable[[int], Awaitable[None]],
    unload_spindle: Callable[[], Awaitable[None]],
    load_defaults: Callable[[], None],
    clear_loaded: Callable[[], None],
) -> dict[int, dict[str, Any]]:
    tool_rows: dict[int, dict[str, Any]] = {}
    with ui.element("div").classes("panel-section"):
        ui.label("Physical Tools").classes("section-title")
        ui.label("C1 rack pockets; CA1 loose-tool drawer. Stickout excludes the 20 mm held in the collet.").classes(
            "section-subtle"
        )
        with ui.element("div").classes("tool-table-wrap"):
            with ui.element("div").classes("tool-table"):
                for heading in ("Pocket", "Present", "Tool", "Stickout", "Tip", "Rack", "Action"):
                    ui.label(heading).classes("table-head")
                defaults = default_tool_map()
                for pocket in sorted(defaults):
                    default = defaults[pocket]
                    ui.label(str(default.get("label", pocket))).classes("table-cell")
                    occupied = ui.switch(value=True).props("dense")
                    tool_number: Any
                    if default.get("locked", False):
                        occupied.disable()
                        tool_number = ui.label(str(default["tool"])).classes("table-cell")
                    else:
                        tool_number = (
                            ui.number(value=default["tool"], min=0, step=1, format="%d")
                            .props("dense outlined")
                            .classes("plain-number")
                        )
                    stickout = (
                        ui.number(value=stickout_from_physical_length(default["length_mm"]), min=0, step=0.5)
                        .props("dense outlined")
                        .classes("plain-number")
                    )
                    probe_tip = (
                        ui.number(value=default["probe_tip_diameter_mm"], min=0, step=0.1)
                        .props("dense outlined")
                        .classes("plain-number")
                    )
                    rack_state = ui.label("not configured").classes("table-cell tool-presence")
                    with ui.element("div").classes("compact-actions"):
                        ui.button("Load", on_click=lambda pocket=pocket: load_pocket(pocket)).props("dense outline")
                    tool_rows[pocket] = {
                        "occupied": occupied,
                        "tool": tool_number,
                        "tool_number": default["tool"],
                        "stickout": stickout,
                        "probe_tip": probe_tip,
                        "rack_state": rack_state,
                    }
        with ui.element("div").classes("button-row"):
            ui.button("Apply Rack", on_click=apply_rack).props("dense color=primary")
            ui.button("C1 Defaults", on_click=load_defaults).props("dense outline")
            ui.button("Empty Rack", on_click=clear_loaded).props("dense outline")
            ui.button("Unload Spindle", on_click=unload_spindle).props("dense outline")

    return tool_rows
