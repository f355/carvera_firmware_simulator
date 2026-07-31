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

import pytest

from gui.protocol.model import Box3D
from gui.scene.machine_visual_spec import C1_VISUAL_SPEC, CA1_VISUAL_SPEC
from gui.scene.scene_geometry import MachineSceneGeometry
from gui.scene.scene_transform import C1_BED_SURFACE_SCENE_Z, SceneTransform, c1_anchors, ca1_anchors
from gui.scene.stock_layer import STOCK_BED_CLEARANCE_MM, STOCK_COLOR, STOCK_OPACITY, StockLayer
from gui.tests.fakes import FakeScene


def test_stock_layer_draws_enabled_stock_on_the_bed_and_tracks_bed_motion() -> None:
    scene = FakeScene()
    physical = Box3D(-303.0, -213.0, -122.0, 1.0, 1.0, 1.0)
    transform = SceneTransform.from_work_area(physical)
    geometry = MachineSceneGeometry(
        machine_model="ca1",
        asset_machine_model="ca1",
        has_split_components=True,
        transform=transform,
        model_offset=CA1_VISUAL_SPEC.model_offset,
        spindle_face_local=CA1_VISUAL_SPEC.spindle_face_local,
    )
    stock = Box3D(-288.669, -201.902, -137.0, -138.669, -51.902, -127.0)
    layer = StockLayer(scene)

    layer.configure(stock, enabled=True, geometry=geometry)

    assert len(scene.box_kwargs) == 1
    assert scene.box_kwargs[0] == pytest.approx({"width": 150.0, "height": 150.0, "depth": 10.0})
    assert scene.boxes[0].materials[-1] == (STOCK_COLOR, STOCK_OPACITY, "both")
    expected_center = ca1_anchors(transform).envelope_point(stock.center_x, stock.center_y, stock.center_z)
    expected_center[2] += STOCK_BED_CLEARANCE_MM
    assert scene.boxes[0].last_move == pytest.approx(tuple(expected_center))

    layer.move(bed_y_delta=12.5)
    assert scene.boxes[0].last_move == pytest.approx(
        (expected_center[0], expected_center[1] + 12.5, expected_center[2])
    )

    layer.configure(stock, enabled=False, geometry=geometry)
    assert scene.boxes[0].deleted is True
    assert layer.object is None


def test_c1_stock_uses_cad_bed_alignment_without_moving_the_spindle_frame() -> None:
    scene = FakeScene()
    physical = Box3D(-372.0, -251.0, -136.0, 1.0, 1.0, 1.0)
    transform = SceneTransform.from_work_area(physical)
    geometry = MachineSceneGeometry(
        machine_model="c1",
        asset_machine_model="c1",
        has_split_components=True,
        transform=transform,
        model_offset=C1_VISUAL_SPEC.model_offset,
        spindle_face_local=C1_VISUAL_SPEC.spindle_face_local,
    )
    stock = Box3D(-359.665, -234.480, -147.5, -209.665, -84.480, -137.5)
    original_spindle_point = c1_anchors().spindle_face_point(-1.0, -1.0, -1.0)

    layer = StockLayer(scene)
    layer.configure(stock, enabled=True, geometry=geometry)

    assert layer.base_position is not None
    visual_bottom_z = layer.base_position[2] - 5.0
    assert visual_bottom_z == pytest.approx(C1_BED_SURFACE_SCENE_Z + STOCK_BED_CLEARANCE_MM)
    assert c1_anchors().spindle_face_point(-1.0, -1.0, -1.0) == original_spindle_point
