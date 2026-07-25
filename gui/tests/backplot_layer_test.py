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
from gui.tests.fakes import FakeScene


def test_backplot_layer_draws_single_green_motion_line_and_clears_it() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=100.0, run_javascript=lambda _script: None)

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
    layer = BackplotLayer(scene=scene, max_speed_mm_s=100.0, run_javascript=lambda _script: None)

    for index in range(500):
        layer.record([float(index), 0.0, 0.0], sample_time_s=float(index) * 0.01)

    assert len(scene.lines) == 1
    assert len(layer.segments) == 1
    assert layer.segments[0].end == [499.0, 0.0, 0.0]


def test_backplot_layer_uses_simulator_sample_time_for_segment_decimation() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, max_speed_mm_s=85.0, run_javascript=lambda _script: None)

    layer.record([0.0, 0.0, 0.0], sample_time_s=10.0)
    layer.record([1.0, 0.0, 0.0], sample_time_s=10.01)

    assert layer.segments[-1].start == [0.0, 0.0, 0.0]
    assert layer.segments[-1].end == [1.0, 0.0, 0.0]


def test_backplot_layer_coalesces_axis_aligned_motion_in_same_direction() -> None:
    scene = FakeScene()
    layer = BackplotLayer(scene=scene, run_javascript=lambda _script: None)

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
    first_layer = BackplotLayer(scene=first_scene, history_store=history, run_javascript=lambda _script: None)

    first_layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    first_layer.record([0.0, 3.0, 0.0], sample_time_s=0.1)
    first_layer.move(y_delta=7.0)

    second_scene = FakeScene()
    second_layer = BackplotLayer(scene=second_scene, history_store=history, run_javascript=lambda _script: None)
    second_layer.restore_from_history()

    assert len(second_scene.lines) == 1
    assert len(second_layer.segments) == 1
    assert second_layer.segments[0].start == [0.0, 0.0, 0.0]
    assert second_layer.segments[0].end == [0.0, 3.0, 0.0]
    assert second_layer.last_point == [0.0, 3.0, 0.0]
    assert second_scene.lines[0].last_move == (0.0, 7.0, 0.0)


def test_backplot_layer_starts_hydrated_so_reload_telemetry_cannot_erase_history() -> None:
    history = BackplotHistoryStore()
    first_layer = BackplotLayer(scene=FakeScene(), history_store=history, run_javascript=lambda _script: None)
    first_layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    first_layer.record([0.0, 3.0, 0.0], sample_time_s=0.1)

    reloaded_layer = BackplotLayer(scene=FakeScene(), history_store=history, run_javascript=lambda _script: None)
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


def recording_layer(**kwargs: object) -> tuple[BackplotLayer, list[str]]:
    scripts: list[str] = []
    layer = BackplotLayer(scene=FakeScene(), run_javascript=scripts.append, **kwargs)  # type: ignore[arg-type]
    return layer, scripts


def op_kinds(scripts: list[str]) -> list[str]:
    kinds = []
    for script in scripts:
        for kind in ("replace_last", "reset", "append"):
            if f'kind: "{kind}"' in script:
                kinds.append(kind)
                break
    return kinds


def test_backplot_sends_incremental_ops_after_the_initial_full_sync() -> None:
    layer, scripts = recording_layer()

    layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    layer.record([0.0, 2.0, 0.0], sample_time_s=0.1)
    layer.record([2.0, 2.0, 0.0], sample_time_s=0.2)
    layer.record([4.0, 2.0, 0.0], sample_time_s=0.3)

    # First render creates the line object and full-syncs; the new segment is
    # an append; the coalesced extension replaces the last segment.
    assert op_kinds(scripts) == ["reset", "append", "replace_last"]
    assert '"base": 1' not in scripts[1]  # base is embedded as plain literal
    assert "base: 1" in scripts[1]
    assert "base: 2" in scripts[2]


def test_backplot_full_syncs_periodically_and_after_pruning() -> None:
    layer, scripts = recording_layer(max_segments=3)

    layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    for index in range(1, 6):
        # Alternate axes so segments cannot coalesce.
        x = float(index) if index % 2 else float(index - 1)
        y = float(index) if not index % 2 else float(index - 1)
        layer.record([x + 2.0 * index, y, 0.0], sample_time_s=float(index))

    kinds = op_kinds(scripts)
    assert kinds[0] == "reset"
    assert "reset" in kinds[1:], "exceeding max_segments must trigger a full sync"
    assert len(layer.segments) == 3


def test_backplot_ops_reset_the_full_sync_counter() -> None:
    layer, scripts = recording_layer()
    layer.ops_since_full_sync = 10**6
    layer.record([0.0, 0.0, 0.0], sample_time_s=0.0)
    layer.record([0.0, 2.0, 0.0], sample_time_s=0.1)
    layer.record([2.0, 2.0, 0.0], sample_time_s=0.2)

    assert op_kinds(scripts) == ["reset", "append"]
    assert layer.ops_since_full_sync == 1
