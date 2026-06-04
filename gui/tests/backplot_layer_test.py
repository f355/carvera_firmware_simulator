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

from gui.scene.backplot_layer import BackplotHistoryStore, BackplotLayer, BackplotSegment


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
    assert layer.segments[0].end == [21.0, 0.0, 0.0]
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
    assert len(layer.segments) == 1
    assert layer.segments[0].end == [499.0, 0.0, 0.0]


def test_backplot_layer_uses_simulator_sample_time_for_segment_decimation() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=85.0)

    layer.record([0.0, 0.0, 0.0], sample_time_s=10.0)
    layer.record([1.0, 0.0, 0.0], sample_time_s=10.01)

    assert layer.segments[-1].start == [0.0, 0.0, 0.0]
    assert layer.segments[-1].end == [1.0, 0.0, 0.0]


def test_backplot_layer_coalesces_axis_aligned_motion_in_same_direction() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene)

    layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    layer.record([0.0, -2.0, 0.0], sample_time_s=0.1)
    layer.record([0.0, -4.0, 0.0], sample_time_s=0.2)
    layer.record([0.0, -5.0, 0.0], sample_time_s=0.3)
    layer.record([0.0, -3.0, 0.0], sample_time_s=0.4)
    layer.record([1.0, -3.0, 0.0], sample_time_s=0.5)

    assert len(layer.segments) == 3
    assert layer.segments[0].start == [0.0, 0.0, 0.0]
    assert layer.segments[0].end == [0.0, -5.0, 0.0]
    assert layer.segments[1].start == [0.0, -5.0, 0.0]
    assert layer.segments[1].end == [0.0, -3.0, 0.0]
    assert layer.segments[2].start == [0.0, -3.0, 0.0]
    assert layer.segments[2].end == [1.0, -3.0, 0.0]


def test_backplot_layer_persists_and_restores_history() -> None:
    history = BackplotHistoryStore()
    first_scene = FakeScene()
    first_layer = BackplotLayer(scene=first_scene, history_store=history)

    first_layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    first_layer.record([0.0, 3.0, 0.0], sample_time_s=0.1)
    first_layer.move(y_delta=7.0)

    second_scene = FakeScene()
    second_layer = BackplotLayer(scene=second_scene, history_store=history)
    second_layer.restore_from_history()

    assert len(second_scene.lines) == 1
    assert len(second_layer.segments) == 1
    assert second_layer.segments[0].start == [0.0, 0.0, 0.0]
    assert second_layer.segments[0].end == [0.0, 3.0, 0.0]
    assert second_layer.last_point == [0.0, 3.0, 0.0]
    assert second_scene.lines[0].last_move == (0.0, 7.0, 0.0)


def test_backplot_layer_starts_hydrated_so_reload_telemetry_cannot_erase_history() -> None:
    history = BackplotHistoryStore()
    first_layer = BackplotLayer(scene=FakeScene(), history_store=history)
    first_layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    first_layer.record([0.0, 3.0, 0.0], sample_time_s=0.1)

    reloaded_layer = BackplotLayer(scene=FakeScene(), history_store=history)
    reloaded_layer.move(y_delta=4.0)

    snapshot = history.snapshot()
    assert len(snapshot.segments) == 1
    assert snapshot.segments[0].start == [0.0, 0.0, 0.0]
    assert snapshot.segments[0].end == [0.0, 3.0, 0.0]
    assert snapshot.y_delta == 4.0


def test_backplot_patch_retries_until_scene_object_exists() -> None:
    from gui.scene.backplot_layer import backplot_patch_javascript

    script = backplot_patch_javascript(
        10,
        20,
        [BackplotSegment(start=[0.0, 0.0, 0.0], end=[1.0, 0.0, 0.0])],
    )

    assert "requestAnimationFrame(() => patchBackplot" in script
    assert "attempt + 1" in script
    assert "missing-object" in script
