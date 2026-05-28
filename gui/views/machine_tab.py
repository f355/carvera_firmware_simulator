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


@dataclass
class FirmwareStateView:
    firmware_badge: Any
    homed_badge: Any
    soft_limit_badge: Any


@dataclass
class MachineTabView:
    front_panel_controls: dict[str, Any] = field(default_factory=dict)
    front_panel_badges: dict[str, Any] = field(default_factory=dict)
    motor_alarm_switches: dict[str, Any] = field(default_factory=dict)
    motor_alarm_badges: dict[str, Any] = field(default_factory=dict)
    spindle_alarm_switch: Any | None = None
    spindle_alarm_badge: Any | None = None
    axis_detail_rows: dict[str, list[Any]] = field(default_factory=dict)
    axis_detail_labels: dict[str, dict[str, Any]] = field(default_factory=dict)
    axis_endstop_badges: dict[str, Any] = field(default_factory=dict)
    cover_switch: Any | None = None
    cover_badge: Any | None = None
    rotary_accessory_switch: Any | None = None
    rotary_accessory_badge: Any | None = None
    probe_badge: Any | None = None
    tool_setter_badge: Any | None = None


def build_firmware_state_panel() -> FirmwareStateView:
    with ui.element("div").classes("panel-section"):
        ui.label("Firmware State").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            with ui.element("div").classes("metric"):
                ui.label("Firmware").classes("metric-name")
                firmware_badge = ui.label("off").classes("badge-off")
            with ui.element("div").classes("metric"):
                ui.label("Homing").classes("metric-name")
                homed_badge = ui.label("not homed").classes("badge-off")
            with ui.element("div").classes("metric"):
                ui.label("Soft limits").classes("metric-name")
                soft_limit_badge = ui.label("disabled").classes("badge-off")
    return FirmwareStateView(
        firmware_badge=firmware_badge,
        homed_badge=homed_badge,
        soft_limit_badge=soft_limit_badge,
    )


def build_machine_tab(
    *,
    cover_changed: Callable[[Any], Awaitable[None]],
    rotary_accessory_changed: Callable[[Any], Awaitable[None]],
) -> MachineTabView:
    front_panel_controls: dict[str, Any] = {}
    front_panel_badges: dict[str, Any] = {}
    motor_alarm_switches: dict[str, Any] = {}
    motor_alarm_badges: dict[str, Any] = {}
    axis_detail_rows: dict[str, list[Any]] = {}
    axis_detail_labels: dict[str, dict[str, Any]] = {}
    axis_endstop_badges: dict[str, Any] = {}

    with ui.element("div").classes("panel-section"):
        ui.label("Inputs").classes("section-title")
        with ui.element("div").classes("input-grid"):
            cover_switch = ui.switch("Cover open", value=False, on_change=cover_changed)
            cover_badge = ui.label("closed").classes("badge-off")
            rotary_accessory_switch = ui.switch("4th axis connected", value=False, on_change=rotary_accessory_changed)
            rotary_accessory_badge = ui.label("not connected").classes("badge-off")
            spindle_alarm_switch = None
        with ui.element("div").classes("contact-grid"):
            ui.label("Probe contact").classes("section-subtle")
            probe_badge = ui.label("open").classes("badge-off")
            ui.label("ETS contact").classes("section-subtle")
            tool_setter_badge = ui.label("open").classes("badge-off")
            ui.label("Spindle alarm").classes("section-subtle")
            spindle_alarm_badge = ui.label("clear").classes("badge-off")

    with ui.element("div").classes("panel-section"):
        ui.label("Axes").classes("section-title")
        with ui.element("div").classes("axis-table"):
            for heading in ("Axis", "Physical", "G53", "Limit"):
                ui.label(heading).classes("table-head")
            for axis, display_name in (("X", "X"), ("Y", "Y"), ("Z", "Z"), ("A", "A"), ("B", "ATC")):
                axis_name_label = ui.label(display_name).classes("table-cell")
                physical_label = ui.label("--").classes("table-cell")
                machine_label = ui.label("--").classes("table-cell")
                endstop_badge = ui.label("clear").classes("badge-off")
                axis_detail_rows[axis] = [
                    axis_name_label,
                    physical_label,
                    machine_label,
                    endstop_badge,
                ]
                axis_detail_labels[axis] = {"physical": physical_label, "machine": machine_label}
                axis_endstop_badges[axis] = endstop_badge

    return MachineTabView(
        front_panel_controls=front_panel_controls,
        front_panel_badges=front_panel_badges,
        motor_alarm_switches=motor_alarm_switches,
        motor_alarm_badges=motor_alarm_badges,
        spindle_alarm_switch=spindle_alarm_switch,
        spindle_alarm_badge=spindle_alarm_badge,
        axis_detail_rows=axis_detail_rows,
        axis_detail_labels=axis_detail_labels,
        axis_endstop_badges=axis_endstop_badges,
        cover_switch=cover_switch,
        cover_badge=cover_badge,
        rotary_accessory_switch=rotary_accessory_switch,
        rotary_accessory_badge=rotary_accessory_badge,
        probe_badge=probe_badge,
        tool_setter_badge=tool_setter_badge,
    )
