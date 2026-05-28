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

from dataclasses import dataclass
from typing import Any

from gui.protocol.model import SpindleSnapshot

TOOL_ROTATION_X = -1.5707963267948966
CA1_SPINDLE_NOSE_DIAMETER_MM = 16.0
SPINDLE_OVERLAY_DIAMETER_MM = CA1_SPINDLE_NOSE_DIAMETER_MM + 1.2
SPINDLE_OVERLAY_RADIUS_MM = SPINDLE_OVERLAY_DIAMETER_MM / 2.0
FALLBACK_SPINDLE_HEIGHT_MM = 28.0
SPINDLE_OVERLAY_IDLE_RGB = (107, 114, 128)
SPINDLE_MAX_RPM_BY_MODEL = {"c1": 15_500.0, "ca1": 14_500.0}


@dataclass
class SpindleOverlayLayer:
    scene: Any
    overlay: Any = None
    tool: Any = None
    tool_length: float | None = None
    latest_spindle: SpindleSnapshot | None = None
    latest_max_rpm: float = 0.0
    current_machine_model: str | None = None

    def reset(self) -> None:
        self.latest_spindle = None
        self.latest_max_rpm = 0.0
        if self.tool is not None:
            self.tool.visible(False)
            self.tool_length = None

    def update_spindle(self, spindle: SpindleSnapshot, machine_model: str | None) -> None:
        self.latest_spindle = spindle
        self.current_machine_model = machine_model
        if spindle.max_rpm > 0.0:
            self.latest_max_rpm = spindle.max_rpm
        self.update_material()

    def set_visible(self, visible: bool) -> None:
        if self.overlay is not None:
            self.overlay.visible(visible)

    def move_overlay(self, spindle_position: list[float], *, visible: bool) -> None:
        self.ensure_overlay()
        if self.overlay is None:
            return
        self.update_material()
        self.overlay.move(
            spindle_position[0],
            spindle_position[1],
            spindle_position[2] + FALLBACK_SPINDLE_HEIGHT_MM / 2.0,
        )
        self.overlay.visible(visible)

    def ensure_overlay(self) -> None:
        if self.scene is None or self.overlay is not None:
            return
        self.overlay = (
            self.scene.cylinder(
                top_radius=SPINDLE_OVERLAY_RADIUS_MM,
                bottom_radius=SPINDLE_OVERLAY_RADIUS_MM,
                height=FALLBACK_SPINDLE_HEIGHT_MM,
                radial_segments=32,
            )
            .material(self.overlay_color())
            .rotate(TOOL_ROTATION_X, 0.0, 0.0)
        )
        self.overlay.visible(False)
        self.update_material()

    def overlay_color(self) -> str:
        spindle = self.latest_spindle
        max_rpm = spindle.max_rpm if spindle is not None else 0.0
        if max_rpm <= 0.0:
            max_rpm = self.latest_max_rpm
        if max_rpm <= 0.0:
            max_rpm = SPINDLE_MAX_RPM_BY_MODEL.get(str(self.current_machine_model), SPINDLE_MAX_RPM_BY_MODEL["c1"])
        actual_rpm = max(0.0, spindle.actual_rpm if spindle is not None else 0.0)
        ratio = min(1.0, actual_rpm / max(1.0, max_rpm))
        base_r, base_g, base_b = SPINDLE_OVERLAY_IDLE_RGB
        red = round(base_r + (255 - base_r) * ratio)
        green = round(base_g * (1.0 - ratio))
        blue = round(base_b * (1.0 - ratio))
        return f"#{red:02x}{green:02x}{blue:02x}"

    def update_material(self) -> None:
        if self.overlay is not None:
            self.overlay.material(self.overlay_color())

    def show_tool(self, spindle_position: list[float], cutting_stickout: float) -> None:
        if self.tool is not None and self.tool_length != cutting_stickout:
            self.tool.delete()
            self.tool = None
        if self.tool is None:
            self.tool = (
                self.scene.cylinder(top_radius=2.0, bottom_radius=2.0, height=cutting_stickout, radial_segments=20)
                .material("#374151")
                .rotate(TOOL_ROTATION_X, 0.0, 0.0)
            )
            self.tool_length = cutting_stickout
        self.tool.move(spindle_position[0], spindle_position[1], spindle_position[2] - cutting_stickout / 2).visible(
            True
        )

    def hide_tool(self) -> None:
        if self.tool is not None:
            self.tool.visible(False)
            self.tool_length = None
