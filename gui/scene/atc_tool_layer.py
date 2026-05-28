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

from gui.protocol.model import AtcSnapshot

from .scene_geometry import MachineSceneGeometry
from .spindle_overlay_layer import TOOL_ROTATION_X


@dataclass
class AtcToolLayer:
    scene: Any
    tool_objects: dict[int, Any] = field(default_factory=dict)

    def reset(self) -> None:
        for tool in self.tool_objects.values():
            tool.visible(False)

    def update(self, atc: AtcSnapshot | None, *, geometry: MachineSceneGeometry, bed_y_delta: float) -> None:
        if geometry.transform is None or atc is None or not atc.available:
            return
        visible_pockets = set()
        for pocket in atc.pockets:
            pocket_id = pocket.pocket
            visible_pockets.add(pocket_id)
            tool_object = self.tool_objects.get(pocket_id)
            if tool_object is None:
                tool_object = (
                    self.scene.cylinder(top_radius=3.0, bottom_radius=3.0, height=20.0, radial_segments=24)
                    .material("#0f766e")
                    .rotate(TOOL_ROTATION_X, 0.0, 0.0)
                )
                self.tool_objects[pocket_id] = tool_object
            rack_position = geometry.atc_rack_tool_position(pocket.x, pocket.y, pocket.z, bed_y_delta)
            tool_object.move(rack_position[0], rack_position[1], rack_position[2] + 10.0).visible(pocket.occupied)
        for pocket_id, tool_object in self.tool_objects.items():
            if pocket_id not in visible_pockets:
                tool_object.visible(False)
