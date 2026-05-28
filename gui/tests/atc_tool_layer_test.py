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

from dataclasses import replace

from gui.core.defaults import C1_SPINDLE_FACE_LOCAL
from gui.protocol.model import AtcPocketSnapshot, AtcSnapshot, Box3D, ToolKind, ToolSnapshot
from gui.scene.atc_tool_layer import AtcToolLayer
from gui.scene.scene_geometry import MachineSceneGeometry
from gui.scene.scene_transform import C1_ATC_RACK_TOP_SCENE_Z, SceneTransform


class FakeObject:
    def __init__(self) -> None:
        self.last_move: tuple[float, float, float] | None = None
        self.visibility: list[bool] = []
        self.deleted = False

    def move(self, x: float, y: float, z: float) -> "FakeObject":
        self.last_move = (x, y, z)
        return self

    def rotate(self, *_values: float) -> "FakeObject":
        return self

    def material(self, *_values: object) -> "FakeObject":
        return self

    def visible(self, value: bool) -> "FakeObject":
        self.visibility.append(value)
        return self

    def delete(self) -> None:
        self.deleted = True


class FakeScene:
    def __init__(self) -> None:
        self.cylinders: list[FakeObject] = []

    def cylinder(self, **_kwargs: object) -> FakeObject:
        obj = FakeObject()
        self.cylinders.append(obj)
        return obj


def _tool() -> ToolSnapshot:
    return ToolSnapshot(
        active_tool=-1,
        target_tool=-1,
        tool_offset_mm=0.0,
        cur_tool_mz=0.0,
        ref_tool_mz=0.0,
        target_collet_type=0,
        length_mm=0.0,
        kind=ToolKind.CUTTING_TOOL,
        probe_tip_diameter_mm=0.0,
    )


def test_atc_tool_layer_places_pocket_tools_from_physical_geometry() -> None:
    scene = FakeScene()
    layer = AtcToolLayer(scene=scene)
    geometry = MachineSceneGeometry(
        machine_model="c1",
        asset_machine_model="c1",
        has_split_components=True,
        transform=SceneTransform.from_work_area(
            Box3D(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
        ),
        model_offset=(-141.5, 13.0, 86.0),
        spindle_face_local=C1_SPINDLE_FACE_LOCAL,
    )
    pocket = AtcPocketSnapshot(
        pocket=1,
        tool=1,
        occupied=True,
        length_mm=40.0,
        x=-4.158,
        y=-234.568,
        z=-112.5,
        kind=ToolKind.CUTTING_TOOL,
        probe_tip_diameter_mm=0.0,
    )

    layer.update(AtcSnapshot(available=True, spindle=_tool(), pockets=(pocket,)), geometry=geometry, bed_y_delta=2.0)

    assert len(scene.cylinders) == 1
    assert scene.cylinders[0].last_move == (172.5, -7.5, C1_ATC_RACK_TOP_SCENE_Z + 10.0)
    assert scene.cylinders[0].visibility[-1] is True

    layer.update(
        AtcSnapshot(available=True, spindle=_tool(), pockets=(replace(pocket, occupied=False),)),
        geometry=geometry,
        bed_y_delta=2.0,
    )
    assert scene.cylinders[0].visibility[-1] is False

    layer.reset()
    assert scene.cylinders[0].visibility[-1] is False
