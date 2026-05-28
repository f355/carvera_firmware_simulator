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

from collections.abc import Mapping, MutableMapping, Sequence
from typing import Any

from gui.core.defaults import DEFAULT_C1_TOOLS, DEFAULT_LOOSE_TOOLS, TOOL_SHANK_INSERT_MM
from gui.protocol.model import ToolConfig, ToolKind


ToolRow = Mapping[str, Any]
MutableToolRow = MutableMapping[str, Any]


def default_tool_map(default_tools: Sequence[Mapping[str, Any]] | None = None) -> dict[int, dict[str, Any]]:
    tools = list(DEFAULT_C1_TOOLS) + list(DEFAULT_LOOSE_TOOLS) if default_tools is None else list(default_tools)
    return {int(tool["pocket"]): dict(tool, occupied=tool.get("occupied", True)) for tool in tools}


def inferred_tool_kind(tool_number: int) -> ToolKind:
    if tool_number == 0:
        return ToolKind.STOCK_Z_PROBE
    if tool_number >= 999990:
        return ToolKind.THREE_AXIS_PROBE
    return ToolKind.CUTTING_TOOL


def physical_length_from_stickout(stickout_mm: float) -> float:
    return max(0.0, stickout_mm) + TOOL_SHANK_INSERT_MM


def stickout_from_physical_length(length_mm: float) -> float:
    return max(0.0, length_mm - TOOL_SHANK_INSERT_MM)


def collect_tool_table(
    tool_rows: Mapping[int, ToolRow],
    *,
    pockets: range | None = None,
    rack_only: bool = False,
) -> list[ToolConfig]:
    defaults = default_tool_map()
    pocket_ids = sorted(defaults) if pockets is None else list(pockets)
    tools: list[ToolConfig] = []
    for pocket in pocket_ids:
        if rack_only and not bool(defaults[pocket].get("rack", True)):
            continue
        row = tool_rows.get(pocket)
        if row is None:
            default_tool = defaults[pocket]
            tools.append(
                ToolConfig(
                    pocket=pocket,
                    tool=int(default_tool["tool"]),
                    length_mm=float(default_tool["length_mm"]),
                    occupied=bool(default_tool.get("occupied", True)),
                    kind=inferred_tool_kind(int(default_tool["tool"])),
                    probe_tip_diameter_mm=float(default_tool.get("probe_tip_diameter_mm", 0.0)),
                )
            )
            continue
        tool_number = int(row.get("tool_number", getattr(row["tool"], "value", 0)) or 0)
        tools.append(
            ToolConfig(
                pocket=pocket,
                tool=tool_number,
                length_mm=physical_length_from_stickout(float(row["stickout"].value or 0.0)),
                occupied=bool(row["occupied"].value),
                kind=inferred_tool_kind(tool_number),
                probe_tip_diameter_mm=float(row["probe_tip"].value or 0.0),
            )
        )
    return tools


def load_default_tools(tool_rows: Mapping[int, MutableToolRow]) -> None:
    defaults = default_tool_map()
    for pocket, row in tool_rows.items():
        row["occupied"].value = True
        if hasattr(row["tool"], "value"):
            row["tool"].value = defaults[pocket]["tool"]
        row["tool_number"] = defaults[pocket]["tool"]
        row["stickout"].value = stickout_from_physical_length(defaults[pocket]["length_mm"])
        if "probe_tip" in row:
            row["probe_tip"].value = defaults[pocket]["probe_tip_diameter_mm"]


def clear_tool_occupancy(tool_rows: Mapping[int, MutableToolRow]) -> None:
    for pocket, row in tool_rows.items():
        if bool(default_tool_map()[pocket].get("locked", False)):
            row["occupied"].value = True
            continue
        row["occupied"].value = False


def collect_box_values(controls: Mapping[str, Any]) -> tuple[float, float, float, float, float, float]:
    return (
        float(controls["min_x"].value or 0.0),
        float(controls["min_y"].value or 0.0),
        float(controls["min_z"].value or 0.0),
        float(controls["max_x"].value or 0.0),
        float(controls["max_y"].value or 0.0),
        float(controls["max_z"].value or 0.0),
    )
