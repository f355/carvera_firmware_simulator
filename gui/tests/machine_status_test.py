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

from gui.protocol.model import AtcPocketSnapshot, AtcSnapshot, AxisSnapshot, MachineState, ToolKind, ToolSnapshot
from gui.tests.fakes import FakeLabel
from gui.views.machine_status import AtcPanelView, AxisPanelView


def test_axis_panel_formats_linear_rotary_and_missing_axes() -> None:
    axis_labels = {"X": FakeLabel(), "Y": FakeLabel(), "A": FakeLabel()}
    axis_details = {
        "X": {"physical": FakeLabel(), "machine": FakeLabel()},
        "Y": {"physical": FakeLabel(), "machine": FakeLabel()},
        "A": {"physical": FakeLabel(), "machine": FakeLabel()},
        "B": {"physical": FakeLabel(), "machine": FakeLabel()},
    }
    axis_badges = {"X": FakeLabel(), "Y": FakeLabel(), "A": FakeLabel(), "B": FakeLabel()}
    axis_view = AxisPanelView(axis_labels, axis_details, axis_badges)

    axis_view.update(
        MachineState(
            firmware_booted=True,
            homed=True,
            soft_endstop_enabled=True,
            work_area=None,
            physical_travel=None,
            axes=(
                AxisSnapshot("X", 0, -2.0, -1.0, True),
                AxisSnapshot("A", 0, 42.0, 41.0, False),
                AxisSnapshot("B", 0, 1.5, 1.0, True),
            ),
            atc=None,
            spindle=None,
            tool_setter=None,
        )
    )
    assert axis_labels["X"].text == "  -2.000 mm"
    assert axis_details["X"]["machine"].text == "-1.000"
    assert axis_badges["X"].text == "HIT"
    assert axis_labels["Y"].text == "--"
    assert axis_labels["A"].text == "  42.000 deg"
    assert axis_details["B"]["physical"].text == "1.500"
    assert axis_badges["B"].text == "HIT"


def test_atc_panel_formats_spindle_and_pocket_state() -> None:
    atc_rows = {1: {"rack_state": FakeLabel()}}
    atc_view = AtcPanelView(
        available_badge=FakeLabel(),
        active_tool_label=FakeLabel(),
        target_tool_label=FakeLabel(),
        tlo_label=FakeLabel(),
        held_length_label=FakeLabel(),
        tool_rows=atc_rows,
    )
    atc_view.update(
        AtcSnapshot(
            available=True,
            spindle=ToolSnapshot(
                active_tool=2,
                target_tool=3,
                tool_offset_mm=-12.3456,
                cur_tool_mz=0.0,
                ref_tool_mz=0.0,
                target_collet_type=0,
                length_mm=42.0,
                kind=ToolKind.CUTTING_TOOL,
                probe_tip_diameter_mm=0.0,
            ),
            pockets=(
                AtcPocketSnapshot(
                    pocket=1,
                    tool=2,
                    occupied=True,
                    length_mm=42.0,
                    x=-1.0,
                    y=-2.0,
                    z=-3.0,
                    kind=ToolKind.CUTTING_TOOL,
                    probe_tip_diameter_mm=0.0,
                ),
            ),
        )
    )
    assert atc_view.available_badge.text == "C1 ATC"
    assert atc_view.tlo_label.text == "-12.346 mm"
    assert atc_rows[1]["rack_state"].text == "occupied"
