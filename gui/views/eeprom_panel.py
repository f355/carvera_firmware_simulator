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

import math
from dataclasses import dataclass, field
from typing import Any, Awaitable, Callable

from nicegui import ui

from gui.protocol.model import EepromContents, PersistentVariable, WorkCoordinateSystem


@dataclass
class WorkCoordinateSystemControls:
    x: Any
    y: Any
    z: Any
    a: Any
    rotation: Any


@dataclass
class EepromPanelView:
    status_label: Any
    fields_container: Any
    tool_length_offset_control: Any | None = None
    reference_machine_z_control: Any | None = None
    tool_machine_z_control: Any | None = None
    reserved_control: Any | None = None
    active_tool_control: Any | None = None
    tool_not_calibrated_control: Any | None = None
    current_wcs_control: Any | None = None
    persistent_variable_controls: dict[int, Any] = field(default_factory=dict)
    work_coordinate_system_controls: dict[int, WorkCoordinateSystemControls] = field(default_factory=dict)

    def set_contents(self, contents: EepromContents) -> None:
        self.fields_container.clear()
        self.persistent_variable_controls.clear()
        self.work_coordinate_system_controls.clear()
        with self.fields_container:
            with self._field_group("Tool State"):
                self.active_tool_control = self._number_control("TOOL", contents.active_tool, integer=True)
                self.tool_length_offset_control = self._number_control("TLO", contents.tool_length_offset)
                self.tool_not_calibrated_control = self._bool_control(
                    "tool_not_calibrated", contents.tool_not_calibrated
                )
                self.reference_machine_z_control = self._number_control("REFMZ", contents.reference_machine_z)
                self.tool_machine_z_control = self._number_control("TOOLMZ", contents.tool_machine_z)
            with self._field_group("Work Offset"):
                self.current_wcs_control = self._number_control("current_wcs", contents.current_wcs, integer=True)
            for system in contents.work_coordinate_systems:
                with self._field_group(f"G{system.number}"):
                    self.work_coordinate_system_controls[system.number] = WorkCoordinateSystemControls(
                        x=self._number_control("X", system.x),
                        y=self._number_control("Y", system.y),
                        z=self._number_control("Z", system.z),
                        a=self._number_control("A", system.a),
                        rotation=self._number_control("rotation", system.rotation),
                    )
            with self._field_group("Persistent Variables"):
                for variable in contents.persistent_variables:
                    self.persistent_variable_controls[variable.number] = self._number_control(
                        str(variable.number), variable.value
                    )
            with self._field_group("Other"):
                self.reserved_control = self._number_control("reserve", contents.reserved)
        field_count = 7 + len(self.persistent_variable_controls) + 5 * len(self.work_coordinate_system_controls)
        self.status_label.text = f"{field_count} EEPROM values loaded"

    def edited_contents(self) -> EepromContents:
        scalar_controls = (
            self.tool_length_offset_control,
            self.reference_machine_z_control,
            self.tool_machine_z_control,
            self.reserved_control,
            self.active_tool_control,
            self.tool_not_calibrated_control,
            self.current_wcs_control,
        )
        if any(control is None for control in scalar_controls):
            raise ValueError("EEPROM contents have not been loaded")
        assert self.tool_length_offset_control is not None
        assert self.reference_machine_z_control is not None
        assert self.tool_machine_z_control is not None
        assert self.reserved_control is not None
        assert self.active_tool_control is not None
        assert self.tool_not_calibrated_control is not None
        assert self.current_wcs_control is not None
        return EepromContents(
            tool_length_offset=float(self.tool_length_offset_control.value or 0.0),
            reference_machine_z=float(self.reference_machine_z_control.value or 0.0),
            tool_machine_z=float(self.tool_machine_z_control.value or 0.0),
            reserved=float(self.reserved_control.value or 0.0),
            active_tool=int(self.active_tool_control.value or 0),
            tool_not_calibrated=bool(self.tool_not_calibrated_control.value),
            current_wcs=int(self.current_wcs_control.value or 0),
            persistent_variables=tuple(
                PersistentVariable(number=number, value=float(control.value or 0.0))
                for number, control in sorted(self.persistent_variable_controls.items())
            ),
            work_coordinate_systems=tuple(
                WorkCoordinateSystem(
                    number=number,
                    x=float(controls.x.value or 0.0),
                    y=float(controls.y.value or 0.0),
                    z=float(controls.z.value or 0.0),
                    a=float(controls.a.value or 0.0),
                    rotation=float(controls.rotation.value or 0.0),
                )
                for number, controls in sorted(self.work_coordinate_system_controls.items())
            ),
        )

    def _field_group(self, title: str) -> Any:
        group = ui.element("div").classes("eeprom-field-group")
        with group:
            ui.label(title).classes("eeprom-group-title")
            grid = ui.element("div").classes("eeprom-fields-grid")
        return grid

    @staticmethod
    def _number_control(name: str, value: float | int, *, integer: bool = False) -> Any:
        ui.label(name).classes("table-cell eeprom-field-name")
        if integer:
            display_value: float | int | None = int(value)
        else:
            float_value = float(value)
            display_value = float_value if math.isfinite(float_value) else None
        return (
            ui.number(
                value=display_value,
                format="%d" if integer else "%.6f",
                step=1 if integer else 0.001,
            )
            .props("dense outlined hide-bottom-space")
            .classes("compact-field plain-number")
        )

    @staticmethod
    def _bool_control(name: str, value: bool) -> Any:
        ui.label(name).classes("table-cell eeprom-field-name")
        return ui.switch(value=value).props("dense")


def build_eeprom_panel(
    *,
    refresh_eeprom: Callable[[], Awaitable[None]],
    write_eeprom: Callable[[], Awaitable[None]],
) -> EepromPanelView:
    with ui.element("div").classes("panel-section"):
        ui.label("EEPROM").classes("section-title")
        with ui.element("div").classes("button-row"):
            ui.button("Refresh", on_click=refresh_eeprom).props("dense outline")
            ui.button("Write contents", on_click=write_eeprom).props("dense color=primary")
        status_label = ui.label("Power on and refresh to view EEPROM contents.").classes("section-subtle")
        fields_container = ui.element("div").classes("eeprom-fields")

    return EepromPanelView(status_label=status_label, fields_container=fields_container)
