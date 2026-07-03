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

from gui.protocol.model import SpindleSnapshot
from gui.scene.spindle_overlay_layer import SPINDLE_OVERLAY_RADIUS_MM, SpindleOverlayLayer
from gui.tests.fakes import FakeScene


def test_spindle_overlay_layer_tints_and_moves_independently() -> None:
    scene = FakeScene()
    layer = SpindleOverlayLayer(scene=scene)

    layer.update_spindle(SpindleSnapshot(False, 5_000.0, 5_000.0, 0.0), machine_model="c1")
    layer.move_overlay([1.0, 2.0, 3.0], visible=True)

    assert scene.cylinder_kwargs[-1]["top_radius"] == SPINDLE_OVERLAY_RADIUS_MM
    assert scene.cylinder_kwargs[-1]["bottom_radius"] == SPINDLE_OVERLAY_RADIUS_MM
    assert scene.cylinders[-1].last_move == (1.0, 2.0, 17.0)
    color_at_5k = scene.cylinders[-1].materials[-1]
    assert color_at_5k != "#ff0000"

    layer.update_spindle(SpindleSnapshot(False, 10_000.0, 10_000.0, 15_500.0), machine_model="c1")
    color_at_10k = scene.cylinders[-1].materials[-1]
    assert color_at_10k != color_at_5k

    layer.update_spindle(SpindleSnapshot(False, 15_500.0, 15_500.0, 15_500.0), machine_model="c1")
    assert scene.cylinders[-1].materials[-1] == "#ff0000"

    layer.show_tool([10.0, 20.0, 30.0], cutting_stickout=25.0)
    tool = scene.cylinders[-1]
    assert scene.cylinder_kwargs[-1]["height"] == 25.0
    assert tool.last_move == (10.0, 20.0, 17.5)
    layer.hide_tool()
    assert tool.visibility[-1] is False
