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

from collections.abc import Mapping
from dataclasses import dataclass
from typing import Any

from gui.protocol.model import AtcSnapshot, MachineState

from .tool_table import stickout_from_physical_length
from .ui_helpers import set_badge


def axis_unit(axis_name: str) -> str:
    return "deg" if axis_name == "A" else "mm"


@dataclass
class AxisPanelView:
    axis_labels: Mapping[str, Any]
    axis_detail_labels: Mapping[str, Mapping[str, Any]]
    axis_endstop_badges: Mapping[str, Any]

    def reset(self) -> None:
        for label in self.axis_labels.values():
            label.text = "--"
        for labels in self.axis_detail_labels.values():
            labels["physical"].text = "--"
            labels["machine"].text = "--"
        for badge in self.axis_endstop_badges.values():
            set_badge(badge, False, "HIT", "clear", warn=True)

    def update(self, state: MachineState) -> None:
        axes = state.axes_by_name()
        for axis_name, label in self.axis_labels.items():
            entry = axes.get(axis_name)
            label.text = "--" if entry is None else f"{entry.physical_mm:8.3f} {axis_unit(axis_name)}"

        for axis_name, labels in self.axis_detail_labels.items():
            entry = axes.get(axis_name)
            if entry is None:
                labels["physical"].text = "--"
                labels["machine"].text = "--"
                set_badge(self.axis_endstop_badges[axis_name], False, "HIT", "clear", warn=True)
                continue
            labels["physical"].text = f"{entry.physical_mm:.3f}"
            labels["machine"].text = f"{entry.machine_position:.3f}"
            set_badge(self.axis_endstop_badges[axis_name], entry.endstop_triggered, "HIT", "clear", warn=True)


@dataclass
class AtcPanelView:
    available_badge: Any
    active_tool_label: Any
    target_tool_label: Any
    tlo_label: Any
    held_length_label: Any
    tool_rows: Mapping[int, Mapping[str, Any]]

    def reset(self) -> None:
        self.update(None)

    def update(self, atc: AtcSnapshot | None) -> None:
        set_badge(self.available_badge, bool(atc.available if atc is not None else False), "C1 ATC", "manual")
        if atc is None:
            self.active_tool_label.text = "-1"
            self.target_tool_label.text = "-1"
            self.tlo_label.text = "0.000 mm"
            self.held_length_label.text = "0.0 mm"
            pockets = {}
        else:
            self.active_tool_label.text = str(atc.spindle.active_tool)
            self.target_tool_label.text = str(atc.spindle.target_tool)
            self.tlo_label.text = f"{atc.spindle.tool_offset_mm:.3f} mm"
            self.held_length_label.text = f"{stickout_from_physical_length(atc.spindle.length_mm):.1f} mm"
            pockets = atc.pockets_by_id()

        for pocket_id, row in self.tool_rows.items():
            pocket = pockets.get(pocket_id)
            if pocket is None:
                row["rack_state"].text = "not configured"
                continue
            row["rack_state"].text = "occupied" if pocket.occupied else "empty"
