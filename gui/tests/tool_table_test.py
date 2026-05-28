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


class FakeControl:
    def __init__(self, value: object = None) -> None:
        self.value = value


def test_tool_table_test() -> None:
    defaults = default_tool_map()
    if set(defaults) != {0, 1, 2, 3, 4, 5, 6, 999999}:
        raise SystemExit("default tool table should expose the C1 rack plus loose probe tools")
    if defaults[0]["tool"] != 0 or not defaults[0]["locked"]:
        raise SystemExit("default tool table should include a locked stock wireless/Z probe")
    if defaults[999999]["tool"] != 999999 or not defaults[999999]["locked"]:
        raise SystemExit("default tool table should include a locked 3D probe")
    if defaults[2]["tool"] != 2 or not defaults[2]["occupied"]:
        raise SystemExit("default C1 tool table should preserve tool numbers and occupancy")
    if inferred_tool_kind(0) != ToolKind.STOCK_Z_PROBE or inferred_tool_kind(999999) != ToolKind.THREE_AXIS_PROBE:
        raise SystemExit("probe tool kinds should be inferred from firmware tool numbers")

    rows = {
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
    table = collect_tool_table(rows, pockets=range(1, 4))
    if table[0] != ToolConfig(
        pocket=1,
        tool=999999,
        length_mm=physical_length_from_stickout(42.5),
        occupied=False,
        kind=ToolKind.THREE_AXIS_PROBE,
        probe_tip_diameter_mm=2.0,
    ):
        raise SystemExit("collect_tool_table should read edited row controls")
    if table[1] != ToolConfig(
        pocket=2,
        tool=0,
        length_mm=physical_length_from_stickout(0.0),
        occupied=True,
        kind=ToolKind.STOCK_Z_PROBE,
        probe_tip_diameter_mm=0.0,
    ):
        raise SystemExit("collect_tool_table should coerce empty control values")
    default_three = ToolConfig(
        pocket=3,
        tool=int(defaults[3]["tool"]),
        length_mm=float(defaults[3]["length_mm"]),
        occupied=bool(defaults[3]["occupied"]),
        kind=ToolKind.CUTTING_TOOL,
        probe_tip_diameter_mm=float(defaults[3]["probe_tip_diameter_mm"]),
    )
    if table[2] != default_three:
        raise SystemExit("collect_tool_table should fill missing rows from defaults with inferred kind")
    rack_table = collect_tool_table(rows, rack_only=True)
    if any(entry.pocket == 999999 for entry in rack_table):
        raise SystemExit("rack-only collection should not send loose probes to the firmware ATC rack")

    load_default_tools(rows)
    if rows[1]["tool"].value != defaults[1]["tool"] or rows[1]["stickout"].value != stickout_from_physical_length(
        defaults[1]["length_mm"]
    ):
        raise SystemExit("load_default_tools should write default tool and stickout controls")
    if rows[1]["probe_tip"].value != defaults[1]["probe_tip_diameter_mm"]:
        raise SystemExit("load_default_tools should write default probe tip controls")
    if rows[1]["occupied"].value is not True:
        raise SystemExit("load_default_tools should mark rows occupied")

    clear_tool_occupancy(rows)
    if rows[1]["occupied"].value is not False or rows[2]["occupied"].value is not False:
        raise SystemExit("clear_tool_occupancy should mark all rows empty")

    box_controls = {
        "min_x": FakeControl("1.5"),
        "min_y": FakeControl(None),
        "min_z": FakeControl("-3"),
        "max_x": FakeControl(4),
        "max_y": FakeControl(""),
        "max_z": FakeControl(6.25),
    }
    if collect_box_values(box_controls) != (1.5, 0.0, -3.0, 4.0, 0.0, 6.25):
        raise SystemExit("collect_box_values should coerce physical box control values")
