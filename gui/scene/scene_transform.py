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

CA1_ETS_MACHINE_XY = (-11.0, -7.0)
# CA1 model-space coordinates for the physical G53 home switch point. The ETS
# top-face center in carvera_air_ca1_y_axis_3.glb sits at this point plus the
# firmware ETS offset.
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


def _ca1_alignment_offset(transform: SceneTransform) -> tuple[float, float]:
    firmware_ets = transform.point(CA1_ETS_MACHINE_XY[0], CA1_ETS_MACHINE_XY[1], transform.bed_z)
    return (
        CA1_ETS_SCENE_XY[0] - firmware_ets[0],
        CA1_ETS_SCENE_XY[1] - firmware_ets[1],
    )


def ca1_bed_scene_point(transform: SceneTransform, x: float, y: float, z: float) -> list[float]:
    """Map firmware coordinates to the CA1 bed/model frame."""
    point = transform.point(x, y, z)
    offset_x, offset_y = _ca1_alignment_offset(transform)
    return [point[0] + offset_x, point[1] + offset_y, point[2] + CA1_BED_SURFACE_SCENE_Z]


def ca1_spindle_face_scene_point(transform: SceneTransform, x: float, y: float, z: float) -> list[float]:
    """Map firmware coordinates to the CA1 moving spindle-face frame."""
    point = transform.point(x, y, z)
    point[2] += CA1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM
    return point


def ca1_envelope_scene_point(transform: SceneTransform, x: float, y: float, z: float) -> list[float]:
    """Draw the CA1 work envelope over the bed, but at the active spindle/tool Z."""
    point = ca1_bed_scene_point(transform, x, y, z)
    point[2] = ca1_spindle_face_scene_point(transform, x, y, z)[2]
    return point


def c1_model_point(x: float, y: float, z: float) -> list[float]:
    home_x, home_y, home_z = C1_HOME_SWITCH_SCENE_XYZ
    return [home_x + x, home_y + y, home_z + z]


def c1_spindle_face_point(x: float, y: float, z: float) -> list[float]:
    point = c1_model_point(x, y, z)
    point[2] += C1_SPINDLE_FACE_SCENE_Z_CORRECTION_MM
    return point
