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

from gui.protocol.model import ToolConfig, ToolKind
from gui.tests.fakes import FakeControl
from gui.views.tool_table import (
    clear_tool_occupancy,
    collect_box_values,
    collect_tool_table,
    default_tool_map,
    inferred_tool_kind,
    load_default_tools,
    physical_length_from_stickout,
    stickout_from_physical_length,
)


def test_default_tool_map_contains_rack_and_probe_tools() -> None:
    defaults = default_tool_map()
    assert set(defaults) == {0, 1, 2, 3, 4, 5, 6, 999999}
    assert (defaults[0]["tool"], defaults[0]["locked"]) == (0, True)
    assert (defaults[999999]["tool"], defaults[999999]["locked"]) == (999999, True)
    assert (defaults[2]["tool"], defaults[2]["occupied"]) == (2, True)
    assert inferred_tool_kind(0) == ToolKind.STOCK_Z_PROBE
    assert inferred_tool_kind(999999) == ToolKind.THREE_AXIS_PROBE


def tool_rows() -> dict[int, dict[str, FakeControl]]:
    return {
        1: {
            "occupied": FakeControl(False),
            "tool": FakeControl(999999),
            "stickout": FakeControl(42.5),
            "probe_tip": FakeControl(2.0),
        },
        2: {
            "occupied": FakeControl(True),
            "tool": FakeControl(""),
            "stickout": FakeControl(None),
            "probe_tip": FakeControl(""),
        },
    }


def test_collect_tool_table_reads_controls_and_fills_missing_pockets() -> None:
    defaults = default_tool_map()
    rows = tool_rows()
    table = collect_tool_table(rows, pockets=range(1, 4))
    assert table[0] == ToolConfig(
        pocket=1,
        tool=999999,
        length_mm=physical_length_from_stickout(42.5),
        occupied=False,
        kind=ToolKind.THREE_AXIS_PROBE,
        probe_tip_diameter_mm=2.0,
    )
    assert table[1] == ToolConfig(
        pocket=2,
        tool=0,
        length_mm=physical_length_from_stickout(0.0),
        occupied=True,
        kind=ToolKind.STOCK_Z_PROBE,
        probe_tip_diameter_mm=0.0,
    )
    default_three = ToolConfig(
        pocket=3,
        tool=int(defaults[3]["tool"]),
        length_mm=float(defaults[3]["length_mm"]),
        occupied=bool(defaults[3]["occupied"]),
        kind=ToolKind.CUTTING_TOOL,
        probe_tip_diameter_mm=float(defaults[3]["probe_tip_diameter_mm"]),
    )
    assert table[2] == default_three
    rack_table = collect_tool_table(rows, rack_only=True)
    assert all(entry.pocket != 999999 for entry in rack_table)


def test_tool_table_mutations_update_controls() -> None:
    defaults = default_tool_map()
    rows = tool_rows()
    load_default_tools(rows)
    assert rows[1]["tool"].value == defaults[1]["tool"]
    assert rows[1]["stickout"].value == stickout_from_physical_length(defaults[1]["length_mm"])
    assert rows[1]["probe_tip"].value == defaults[1]["probe_tip_diameter_mm"]
    assert rows[1]["occupied"].value is True

    clear_tool_occupancy(rows)
    assert rows[1]["occupied"].value is False
    assert rows[2]["occupied"].value is False


def test_collect_box_values_coerces_empty_and_numeric_controls() -> None:
    box_controls = {
        "min_x": FakeControl("1.5"),
        "min_y": FakeControl(None),
        "min_z": FakeControl("-3"),
        "max_x": FakeControl(4),
        "max_y": FakeControl(""),
        "max_z": FakeControl(6.25),
    }
    assert collect_box_values(box_controls) == (1.5, 0.0, -3.0, 4.0, 0.0, 6.25)
