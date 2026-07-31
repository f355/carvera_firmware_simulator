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

from dataclasses import dataclass
from typing import Any

from gui.protocol.model import Box3D

from .scene_geometry import MachineSceneGeometry

STOCK_COLOR = "#b7793f"
STOCK_OPACITY = 0.78
STOCK_BED_CLEARANCE_MM = 0.2


@dataclass
class StockLayer:
    scene: Any
    box: Box3D | None = None
    enabled: bool = False
    object: Any = None
    base_position: tuple[float, float, float] | None = None

    def configure(self, box: Box3D, *, enabled: bool, geometry: MachineSceneGeometry) -> None:
        self.box = box
        self.enabled = enabled
        self.redraw(geometry)

    def reset(self) -> None:
        if self.object is not None:
            self.object.visible(False)

    def redraw(self, geometry: MachineSceneGeometry) -> None:
        if self.object is not None:
            self.object.delete()
            self.object = None
        self.base_position = None
        if not self.enabled or self.box is None or geometry.transform is None or self.scene is None:
            return

        min_x, max_x = sorted((self.box.min_x, self.box.max_x))
        min_y, max_y = sorted((self.box.min_y, self.box.max_y))
        min_z, max_z = sorted((self.box.min_z, self.box.max_z))
        minimum = geometry.bed_point(min_x, min_y, min_z)
        maximum = geometry.bed_point(max_x, max_y, max_z)
        self.base_position = (
            (minimum[0] + maximum[0]) / 2.0,
            (minimum[1] + maximum[1]) / 2.0,
            (minimum[2] + maximum[2]) / 2.0 + STOCK_BED_CLEARANCE_MM,
        )
        self.object = self.scene.box(
            width=abs(maximum[0] - minimum[0]),
            height=abs(maximum[1] - minimum[1]),
            depth=abs(maximum[2] - minimum[2]),
        ).material(STOCK_COLOR, STOCK_OPACITY, "both")
        self.object.move(*self.base_position)

    def move(self, *, bed_y_delta: float) -> None:
        if self.object is None or self.base_position is None:
            return
        self.object.move(
            self.base_position[0],
            self.base_position[1] + bed_y_delta,
            self.base_position[2],
        )
