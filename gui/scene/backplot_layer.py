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

from collections.abc import Callable
from dataclasses import dataclass, field
from math import dist
from threading import Lock
from typing import Any

from nicegui import ui

BACKPLOT_LINE_COLOR = "#16a34a"
BACKPLOT_MIN_DISTANCE_MM = 1.0
BACKPLOT_MAX_SEGMENTS = 2000
BACKPLOT_AXIS_EPSILON_MM = 1e-6
BACKPLOT_PATCH_MAX_FRAMES = 120


@dataclass(frozen=True, slots=True)
class BackplotSegment:
    start: list[float]
    end: list[float]


@dataclass(frozen=True, slots=True)
class BackplotHistorySnapshot:
    segments: tuple[BackplotSegment, ...] = ()
    last_point: list[float] | None = None
    last_sample_time_s: float | None = None
    y_delta: float = 0.0


class BackplotHistoryStore:
    def __init__(self) -> None:
        self._lock = Lock()
        self._snapshot = BackplotHistorySnapshot()

    def snapshot(self) -> BackplotHistorySnapshot:
        with self._lock:
            return BackplotHistorySnapshot(
                segments=tuple(
                    BackplotSegment(list(segment.start), list(segment.end)) for segment in self._snapshot.segments
                ),
                last_point=list(self._snapshot.last_point) if self._snapshot.last_point is not None else None,
                last_sample_time_s=self._snapshot.last_sample_time_s,
                y_delta=self._snapshot.y_delta,
            )

    def update(
        self,
        *,
        segments: list[BackplotSegment],
        last_point: list[float] | None,
        last_sample_time_s: float | None,
        y_delta: float,
    ) -> None:
        with self._lock:
            self._snapshot = BackplotHistorySnapshot(
                segments=tuple(BackplotSegment(list(segment.start), list(segment.end)) for segment in segments),
                last_point=list(last_point) if last_point is not None else None,
                last_sample_time_s=last_sample_time_s,
                y_delta=y_delta,
            )

    def clear(self) -> None:
        with self._lock:
            self._snapshot = BackplotHistorySnapshot()


@dataclass
class BackplotLayer:
    scene: Any
    max_speed_mm_s: float = 0.0
    min_distance_mm: float = BACKPLOT_MIN_DISTANCE_MM
    max_segments: int = BACKPLOT_MAX_SEGMENTS
    history_store: BackplotHistoryStore | None = None
    run_javascript: Callable[[str], Any] = ui.run_javascript
    segments: list[BackplotSegment] = field(default_factory=list)
    line_object: Any = None
    last_point: list[float] | None = None
    last_sample_time_s: float | None = None
    y_delta: float = 0.0

    def __post_init__(self) -> None:
        self._load_history_snapshot()

    def clear(self) -> None:
        if self.line_object is not None:
            self.line_object.delete()
        self.line_object = None
        self.segments = []
        self.last_point = None
        self.last_sample_time_s = None
        self.y_delta = 0.0
        self._store_history()

    def reset_position(self) -> None:
        self.last_point = None
        self.last_sample_time_s = None

    def record(self, point: list[float], *, sample_time_s: float) -> None:
        if self.scene is None:
            return
        current = list(point)
        if self.last_point is None or self.last_sample_time_s is None:
            self.last_point = current
            self.last_sample_time_s = sample_time_s
            return

        distance_mm = dist(self.last_point, current)
        if distance_mm < self.min_distance_mm:
            return

        if self._coalesce_last_segment(current):
            self.last_point = current
            self.last_sample_time_s = sample_time_s
            self._render()
            self._store_history()
            return

        self.segments.append(BackplotSegment(start=self.last_point, end=current))
        if len(self.segments) > self.max_segments:
            self.segments = self.segments[-self.max_segments :]
        self.last_point = current
        self.last_sample_time_s = sample_time_s
        self._render()
        self._store_history()

    def move(self, *, y_delta: float) -> None:
        self.y_delta = y_delta
        if self.line_object is not None and hasattr(self.line_object, "move"):
            self.line_object.move(0.0, y_delta, 0.0)
        self._store_history()

    def restore_from_history(self) -> None:
        if not self._load_history_snapshot():
            return
        if self.segments:
            self._render()
            self.move(y_delta=self.y_delta)

    def _ensure_line_object(self) -> Any | None:
        if self.line_object is None:
            self.line_object = self.scene.line([0.0, 0.0, 0.0], [0.0, 0.0, 0.0]).material(BACKPLOT_LINE_COLOR)
        return self.line_object

    def _render(self) -> None:
        line_object = self._ensure_line_object()
        if line_object is None:
            return
        if not (hasattr(self.scene, "id") and hasattr(line_object, "id")):
            return
        self.run_javascript(backplot_patch_javascript(self.scene.id, line_object.id, self.segments))

    def _coalesce_last_segment(self, current: list[float]) -> bool:
        if not self.segments:
            return False

        previous = self.segments[-1]
        previous_axis = axis_aligned_delta(previous.start, previous.end)
        current_axis = axis_aligned_delta(previous.end, current)
        if previous_axis is None or current_axis is None:
            return False

        previous_index, previous_direction = previous_axis
        current_index, current_direction = current_axis
        if previous_index != current_index or previous_direction != current_direction:
            return False

        self.segments[-1] = BackplotSegment(start=previous.start, end=current)
        return True

    def _store_history(self) -> None:
        if self.history_store is None:
            return
        self.history_store.update(
            segments=self.segments,
            last_point=self.last_point,
            last_sample_time_s=self.last_sample_time_s,
            y_delta=self.y_delta,
        )

    def _load_history_snapshot(self) -> bool:
        if self.history_store is None:
            return False
        snapshot = self.history_store.snapshot()
        self.segments = [BackplotSegment(list(segment.start), list(segment.end)) for segment in snapshot.segments]
        self.last_point = list(snapshot.last_point) if snapshot.last_point is not None else None
        self.last_sample_time_s = snapshot.last_sample_time_s
        self.y_delta = snapshot.y_delta
        return True


def axis_aligned_delta(start: list[float], end: list[float]) -> tuple[int, int] | None:
    moving_axes: list[tuple[int, int]] = []
    for index, (start_value, end_value) in enumerate(zip(start, end, strict=True)):
        delta = end_value - start_value
        if abs(delta) <= BACKPLOT_AXIS_EPSILON_MM:
            continue
        moving_axes.append((index, 1 if delta > 0.0 else -1))

    if len(moving_axes) != 1:
        return None
    return moving_axes[0]


def backplot_patch_javascript(scene_id: int, line_object_id: int, segments: list[BackplotSegment]) -> str:
    positions: list[float] = []
    for segment in segments:
        positions.extend((*segment.start, *segment.end))
    positions_literal = "[" + ",".join(f"{value:.6f}" for value in positions) + "]"
    return f"""
(() => {{
  const positions = {positions_literal};
  const maxFrames = {BACKPLOT_PATCH_MAX_FRAMES};
  const findBackplotObject = () => {{
    const root = document.getElementById("c{scene_id}");
    const element = typeof getElement === "function" ? getElement("{scene_id}") : null;
    let object = element?.objects?.get("{line_object_id}");
    if (!object) {{
      const scene = root ? window["scene_" + root.id] : null;
      scene?.traverse((candidate) => {{
        if (!object && candidate.object_id === "{line_object_id}") {{
          object = candidate;
        }}
      }});
    }}
    return {{ root, object }};
  }};
  const patchBackplot = (attempt = 0) => {{
    const {{ root, object }} = findBackplotObject();
    if (!object?.geometry?.setAttribute || !object?.material) {{
      if (attempt < maxFrames) {{
        requestAnimationFrame(() => patchBackplot(attempt + 1));
        return;
      }}
      if (root) {{
        root.dataset.carveraBackplot = "missing-object";
        root.dataset.carveraBackplotAttempts = String(attempt);
      }}
      return;
    }}
    const attributeCtor = object.geometry.attributes.position?.constructor;
    if (!attributeCtor) {{
      if (root) {{
        root.dataset.carveraBackplot = "missing-attribute-constructor";
      }}
      return;
    }}
    object.geometry.setAttribute("position", new attributeCtor(positions, 3));
    object.geometry.attributes.position.needsUpdate = true;
    object.geometry.deleteAttribute?.("color");
    object.geometry.computeBoundingSphere();
    object.material.color?.set?.("{BACKPLOT_LINE_COLOR}");
    object.material.vertexColors = false;
    object.material.needsUpdate = true;
    object.visible = positions.length > 0;
    if (root) {{
      root.dataset.carveraBackplot = "patched";
      root.dataset.carveraBackplotAttempts = String(attempt);
      root.dataset.carveraBackplotSegments = String(positions.length / 6);
    }}
  }};
  patchBackplot();
}})();
"""
