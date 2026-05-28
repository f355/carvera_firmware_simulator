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

from dataclasses import dataclass, field
from typing import Any, Awaitable, Callable

from nicegui import ui

from gui.protocol.model import EepromField, EepromFieldType


FIELD_GROUPS = (
    ("Tool State", ("TOOL", "TLO", "tool_not_calibrated", "REFMZ", "TOOLMZ")),
    ("Work Offset", ("current_wcs",)),
    ("G54", ("G54.X", "G54.Y", "G54.Z", "G54.A", "G54.rotation")),
    ("G55", ("G55.X", "G55.Y", "G55.Z", "G55.A", "G55.rotation")),
    ("G56", ("G56.X", "G56.Y", "G56.Z", "G56.A", "G56.rotation")),
    ("G57", ("G57.X", "G57.Y", "G57.Z", "G57.A", "G57.rotation")),
    ("G58", ("G58.X", "G58.Y", "G58.Z", "G58.A", "G58.rotation")),
    ("G59", ("G59.X", "G59.Y", "G59.Z", "G59.A", "G59.rotation")),
    ("Persistent Variables", tuple(f"perm_vars[{index}]" for index in range(501, 521))),
    ("Other", ()),
)


@dataclass
class EepromPanelView:
    status_label: Any
    fields_container: Any
    field_controls: dict[str, tuple[EepromFieldType, Any]] = field(default_factory=dict)

    def set_fields(self, fields: list[EepromField]) -> None:
        self.fields_container.clear()
        self.field_controls.clear()
        fields_by_name = {field.name: field for field in fields}
        rendered = set()
        with self.fields_container:
            for group_name, names in FIELD_GROUPS:
                group_fields = [fields_by_name[name] for name in names if name in fields_by_name]
                if group_name == "Other":
                    group_fields = [field for field in fields if field.name not in rendered]
                if not group_fields:
                    continue
                with ui.element("div").classes("eeprom-field-group"):
                    ui.label(group_name).classes("eeprom-group-title")
                    with ui.element("div").classes("eeprom-fields-grid"):
                        ui.label("Field").classes("table-head")
                        ui.label("Value").classes("table-head")
                        for field in group_fields:
                            self._add_field_control(field)
                            rendered.add(field.name)
        self.status_label.text = f"{len(fields)} EEPROM fields loaded"

    def edited_fields(self) -> list[EepromField]:
        fields = []
        for name, (kind, control) in self.field_controls.items():
            fields.append(EepromField(name=name, type=kind, value=control.value))
        return fields

    def _add_field_control(self, field: EepromField) -> None:
        name = field.name
        kind = field.type
        ui.label(name).classes("table-cell eeprom-field-name")
        control: Any
        if kind == EepromFieldType.BOOL:
            control = ui.switch(value=bool(field.value)).props("dense")
        elif kind == EepromFieldType.INT:
            control = (
                ui.number(value=int(field.value), step=1)
                .props("dense outlined hide-bottom-space")
                .classes("compact-field plain-number")
            )
        else:
            control = (
                ui.number(value=float(field.value), format="%.6f", step=0.001)
                .props("dense outlined hide-bottom-space")
                .classes("compact-field plain-number")
            )
        self.field_controls[name] = (kind, control)


def build_eeprom_panel(
    *,
    refresh_eeprom: Callable[[], Awaitable[None]],
    write_eeprom: Callable[[], Awaitable[None]],
) -> EepromPanelView:
    with ui.element("div").classes("panel-section"):
        ui.label("EEPROM").classes("section-title")
        with ui.element("div").classes("button-row"):
            ui.button("Refresh", on_click=refresh_eeprom).props("dense outline")
            ui.button("Write fields", on_click=write_eeprom).props("dense color=primary")
        status_label = ui.label("Power on and refresh to view named EEPROM fields.").classes("section-subtle")
        fields_container = ui.element("div").classes("eeprom-fields")

    return EepromPanelView(status_label=status_label, fields_container=fields_container)
