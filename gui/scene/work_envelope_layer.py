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
from typing import Any

from gui.protocol.model import Box3D

from .scene_geometry import MachineSceneGeometry

BOX_EDGES = (
    ("000", "100"),
    ("100", "110"),
    ("110", "010"),
    ("010", "000"),
    ("001", "101"),
    ("101", "111"),
    ("111", "011"),
    ("011", "001"),
    ("000", "001"),
    ("100", "101"),
    ("110", "111"),
    ("010", "011"),
)


@dataclass
class WorkEnvelopeLayer:
    scene: Any
    key: object | None = None
    line_objects: list[Any] = field(default_factory=list)

    def clear(self) -> None:
        for line in self.line_objects:
            line.delete()
        self.line_objects = []
        self.key = None

    def redraw(
        self, physical_box: Box3D, soft_box: Box3D | None, *, key: object, geometry: MachineSceneGeometry
    ) -> None:
        self.clear()
        self.key = key
        self.line_objects.extend(self.draw_box_lines(physical_box, "#dc2626", geometry))
        if soft_box is not None:
            self.line_objects.extend(self.draw_box_lines(soft_box, "#2563eb", geometry))

    def move(self, *, y_delta: float, z_delta: float) -> None:
        for line in self.line_objects:
            line.move(0.0, y_delta, z_delta)

    def draw_box_lines(self, box: Box3D, color: str, geometry: MachineSceneGeometry) -> list[Any]:
        min_x, max_x = sorted((box.min_x, box.max_x))
        min_y, max_y = sorted((box.min_y, box.max_y))
        min_z, max_z = sorted((box.min_z, box.max_z))
        corners = {
            "000": geometry.active_envelope_point(min_x, min_y, min_z),
            "100": geometry.active_envelope_point(max_x, min_y, min_z),
            "110": geometry.active_envelope_point(max_x, max_y, min_z),
            "010": geometry.active_envelope_point(min_x, max_y, min_z),
            "001": geometry.active_envelope_point(min_x, min_y, max_z),
            "101": geometry.active_envelope_point(max_x, min_y, max_z),
            "111": geometry.active_envelope_point(max_x, max_y, max_z),
            "011": geometry.active_envelope_point(min_x, max_y, max_z),
        }
        return [self.scene.line(corners[start], corners[end]).material(color) for start, end in BOX_EDGES]
