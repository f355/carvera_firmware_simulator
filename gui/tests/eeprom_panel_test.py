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

from types import SimpleNamespace

from gui.protocol.model import EepromContents, PersistentVariable, WorkCoordinateSystem
from gui.views.eeprom_panel import EepromPanelView, WorkCoordinateSystemControls


def control(value):
    return SimpleNamespace(value=value)


def test_eeprom_panel_builds_structured_contents_without_parsing_labels() -> None:
    view = EepromPanelView(status_label=SimpleNamespace(), fields_container=SimpleNamespace())
    view.tool_length_offset_control = control(1.0)
    view.reference_machine_z_control = control(2.0)
    view.tool_machine_z_control = control(3.0)
    view.reserved_control = control(4.0)
    view.active_tool_control = control(5)
    view.tool_not_calibrated_control = control(True)
    view.current_wcs_control = control(6)
    view.persistent_variable_controls = {501: control(7.0)}
    view.work_coordinate_system_controls = {
        54: WorkCoordinateSystemControls(
            x=control(8.0),
            y=control(9.0),
            z=control(10.0),
            a=control(11.0),
            rotation=control(12.0),
        )
    }

    assert view.edited_contents() == EepromContents(
        tool_length_offset=1.0,
        reference_machine_z=2.0,
        tool_machine_z=3.0,
        reserved=4.0,
        active_tool=5,
        tool_not_calibrated=True,
        current_wcs=6,
        persistent_variables=(PersistentVariable(number=501, value=7.0),),
        work_coordinate_systems=(WorkCoordinateSystem(number=54, x=8.0, y=9.0, z=10.0, a=11.0, rotation=12.0),),
    )
