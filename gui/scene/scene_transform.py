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

from gui.protocol.model import Box3D

# Firmware G53 offset of the CA1 ETS top-face center; the home-switch anchor
# below was measured by aligning this point with the ETS mesh in
# carvera_air_ca1_y_axis_3.glb (see scripts/model_tools/analyze_glb_geometry.py).
CA1_ETS_MACHINE_XY = (-11.0, -7.0)
# CA1 model-space coordinates for the physical G53 home switch point.
CA1_HOME_SWITCH_SCENE_XY = (153.016, 107.096)
CA1_ETS_SCENE_XY = (
    CA1_HOME_SWITCH_SCENE_XY[0] + CA1_ETS_MACHINE_XY[0],
    CA1_HOME_SWITCH_SCENE_XY[1] + CA1_ETS_MACHINE_XY[1],
)
CA1_BED_SURFACE_SCENE_Z = -22.0
CA1_ETS_TOP_SCENE_Z = 0.0

# C1 model-space coordinates for the physical G53 home switch point. Z is
# visually aligned to the real work volume; the GLB omits pocket depth.
C1_HOME_SWITCH_SCENE_XYZ = (176.658, 225.068, 160.5)
C1_ATC_RACK_TOP_SCENE_Z = 63.0
C1_BED_MESH_Y_ALIGNMENT_MM = 13.7
# Measured from the transformed C1 Y-axis GLB. Bed-mounted geometry uses this
# independently of the spindle and envelope frames.
C1_BED_SURFACE_SCENE_Z = 31.0
CA1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM = -7.0
C1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM = 18.0


@dataclass(frozen=True)
class SceneTransform:
    center_x: float
    center_y: float
    bed_z: float

    @classmethod
    def from_work_area(cls, work_area: Box3D) -> SceneTransform:
        min_x, max_x = sorted((work_area.min_x, work_area.max_x))
        min_y, max_y = sorted((work_area.min_y, work_area.max_y))
        min_z, _ = sorted((work_area.min_z, work_area.max_z))
        return cls(
            center_x=(min_x + max_x) / 2,
            center_y=(min_y + max_y) / 2,
            bed_z=min_z,
        )

    def point(self, x: float, y: float, z: float) -> list[float]:
        return [
            x - self.center_x,
            y - self.center_y,
            z - self.bed_z,
        ]


@dataclass(frozen=True)
class FrameAnchors:
    """Additive anchors mapping firmware G53 coordinates into scene frames.

    Every placement is anchor + (x, y, z); machines differ only in where each
    frame's anchor sits, not in the mapping itself. The envelope frame takes
    its XY from the model frame and its Z from the spindle-face frame, so the
    envelope hugs the bed while tracking the active tool height.
    """

    model: tuple[float, float, float]
    spindle_face: tuple[float, float, float]

    def model_point(self, x: float, y: float, z: float) -> list[float]:
        return [self.model[0] + x, self.model[1] + y, self.model[2] + z]

    def spindle_face_point(self, x: float, y: float, z: float) -> list[float]:
        return [self.spindle_face[0] + x, self.spindle_face[1] + y, self.spindle_face[2] + z]

    def envelope_point(self, x: float, y: float, z: float) -> list[float]:
        return [self.model[0] + x, self.model[1] + y, self.spindle_face[2] + z]


def c1_anchors() -> FrameAnchors:
    home_x, home_y, home_z = C1_HOME_SWITCH_SCENE_XYZ
    return FrameAnchors(
        model=C1_HOME_SWITCH_SCENE_XYZ,
        spindle_face=(home_x, home_y, home_z + C1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM),
    )


def ca1_anchors(transform: SceneTransform) -> FrameAnchors:
    # Historically the CA1 bed frame routed through the work-area-centered
    # transform and re-added an ETS alignment offset; the centering terms
    # cancel, leaving home-switch + firmware coordinates exactly like the C1.
    # Only the Z anchor depends on the work area, and only the moving
    # spindle-face frame stays work-area-centered.
    return FrameAnchors(
        model=(
            CA1_HOME_SWITCH_SCENE_XY[0],
            CA1_HOME_SWITCH_SCENE_XY[1],
            CA1_BED_SURFACE_SCENE_Z - transform.bed_z,
        ),
        spindle_face=(
            -transform.center_x,
            -transform.center_y,
            CA1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM - transform.bed_z,
        ),
    )


def transform_anchors(transform: SceneTransform) -> FrameAnchors:
    """Fallback frame for machines without a calibrated model: all frames coincide."""
    anchor = (-transform.center_x, -transform.center_y, -transform.bed_z)
    return FrameAnchors(model=anchor, spindle_face=anchor)
