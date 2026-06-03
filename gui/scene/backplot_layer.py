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
from math import dist
from typing import Any, Callable

from nicegui import ui


BACKPLOT_LINE_COLOR = "#16a34a"
BACKPLOT_MIN_DISTANCE_MM = 1.0
BACKPLOT_MAX_SEGMENTS = 2000


@dataclass(frozen=True, slots=True)
class BackplotSegment:
    start: list[float]
    end: list[float]


@dataclass
class BackplotLayer:
    scene: Any
    max_speed_mm_s: float = 0.0
    min_distance_mm: float = BACKPLOT_MIN_DISTANCE_MM
    max_segments: int = BACKPLOT_MAX_SEGMENTS
    run_javascript: Callable[[str], Any] = ui.run_javascript
    segments: list[BackplotSegment] = field(default_factory=list)
    line_object: Any = None
    last_point: list[float] | None = None
    last_sample_time_s: float | None = None
    y_delta: float = 0.0

    def clear(self) -> None:
        if self.line_object is not None:
            self.line_object.delete()
        self.line_object = None
        self.segments = []
        self.last_point = None
        self.last_sample_time_s = None
        self.y_delta = 0.0

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

        self.segments.append(BackplotSegment(start=self.last_point, end=current))
        if len(self.segments) > self.max_segments:
            self.segments = self.segments[-self.max_segments :]
        self.last_point = current
        self.last_sample_time_s = sample_time_s
        self._render()

    def move(self, *, y_delta: float) -> None:
        self.y_delta = y_delta
        if self.line_object is not None and hasattr(self.line_object, "move"):
            self.line_object.move(0.0, y_delta, 0.0)

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


def backplot_patch_javascript(scene_id: int, line_object_id: int, segments: list[BackplotSegment]) -> str:
    positions: list[float] = []
    for segment in segments:
        positions.extend((*segment.start, *segment.end))
    positions_literal = "[" + ",".join(f"{value:.6f}" for value in positions) + "]"
    return f"""
(() => {{
  const positions = {positions_literal};
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
  if (!object?.geometry?.setAttribute || !object?.material) {{
    if (root) {{
      root.dataset.carveraBackplot = "missing-object";
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
    root.dataset.carveraBackplotSegments = String(positions.length / 6);
  }}
}})();
"""
