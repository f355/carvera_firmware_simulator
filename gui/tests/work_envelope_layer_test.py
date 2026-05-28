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

from gui.protocol.model import Box3D
from gui.scene.scene_geometry import MachineSceneGeometry
from gui.scene.scene_transform import SceneTransform, c1_spindle_face_point
from gui.scene.work_envelope_layer import WorkEnvelopeLayer


class FakeLine:
    def __init__(self, start: list[float], end: list[float]) -> None:
        self.start = start
        self.end = end
        self.color = ""
        self.last_move: tuple[float, float, float] | None = None
        self.deleted = False

    def material(self, color: str) -> "FakeLine":
        self.color = color
        return self

    def move(self, x: float, y: float, z: float) -> None:
        self.last_move = (x, y, z)

    def delete(self) -> None:
        self.deleted = True


class FakeScene:
    def __init__(self) -> None:
        self.lines: list[FakeLine] = []

    def line(self, start: list[float], end: list[float]) -> FakeLine:
        line = FakeLine(start, end)
        self.lines.append(line)
        return line


def test_work_envelope_layer_draws_and_moves_physical_and_soft_boxes() -> None:
    scene = FakeScene()
    physical = Box3D(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
    soft = Box3D(min_x=-371.0, min_y=-250.0, min_z=-135.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
    geometry = MachineSceneGeometry(
        machine_model="c1",
        asset_machine_model="c1",
        has_split_components=True,
        transform=SceneTransform.from_work_area(physical),
        model_offset=(-141.5, 13.0, 86.0),
        spindle_face_local=(-29.954, -19.092, 66.625),
    )
    layer = WorkEnvelopeLayer(scene=scene)

    layer.redraw(physical, soft, key=("physical", "soft"), geometry=geometry)

    assert layer.key == ("physical", "soft")
    assert len(scene.lines) == 24
    assert {line.color for line in scene.lines} == {"#dc2626", "#2563eb"}
    assert any(line.end == c1_spindle_face_point(1.0, 1.0, 1.0) for line in scene.lines)

    layer.move(y_delta=12.0, z_delta=-25.0)
    assert all(line.last_move == (0.0, 12.0, -25.0) for line in scene.lines)

    first_line = scene.lines[0]
    layer.clear()
    assert first_line.deleted is True
    assert layer.line_objects == []
    assert layer.key is None
