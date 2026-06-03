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

from gui.scene.backplot_layer import BackplotLayer


class FakeLine:
    def __init__(self, start: list[float], end: list[float]) -> None:
        self.start = start
        self.end = end
        self.color = ""
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


def test_backplot_layer_draws_single_green_motion_line_and_clears_it() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=100.0)

    layer.record([0.0, 0.0, 0.0], sample_time_s=10.0)
    layer.record([1.0, 0.0, 0.0], sample_time_s=10.1)
    layer.record([21.0, 0.0, 0.0], sample_time_s=10.2)
    layer.record([21.0001, 0.0, 0.0], sample_time_s=10.3)

    assert len(scene.lines) == 1
    assert layer.segments[0].start == [0.0, 0.0, 0.0]
    assert layer.segments[0].end == [1.0, 0.0, 0.0]
    assert scene.lines[0].color == "#16a34a"

    first_line = scene.lines[0]
    layer.clear()

    assert first_line.deleted is True
    assert layer.segments == []


def test_backplot_layer_does_not_create_one_scene_object_per_segment() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=100.0)

    for index in range(500):
        layer.record([float(index), 0.0, 0.0], sample_time_s=float(index) * 0.01)

    assert len(scene.lines) == 1
    assert len(layer.segments) > 100


def test_backplot_layer_uses_simulator_sample_time_for_segment_decimation() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=85.0)

    layer.record([0.0, 0.0, 0.0], sample_time_s=10.0)
    layer.record([1.0, 0.0, 0.0], sample_time_s=10.01)

    assert layer.segments[-1].start == [0.0, 0.0, 0.0]
    assert layer.segments[-1].end == [1.0, 0.0, 0.0]
