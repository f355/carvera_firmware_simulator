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

from .scene_transform import (
    C1_ATC_RACK_TOP_SCENE_Z,
    C1_BED_MESH_Y_ALIGNMENT_MM,
    SceneTransform,
    ca1_bed_scene_point,
    ca1_envelope_scene_point,
    ca1_spindle_face_scene_point,
    c1_model_point,
    c1_spindle_face_point,
)

Vector3 = tuple[float, float, float]


@dataclass(frozen=True)
class AxisComponentPositions:
    bed_y_delta: float
    positions: dict[str, Vector3]


@dataclass(frozen=True)
class MachineSceneGeometry:
    machine_model: str | None
    asset_machine_model: str | None
    has_split_components: bool
    transform: SceneTransform | None
    model_offset: Vector3 = (0.0, 0.0, 0.0)
    spindle_face_local: Vector3 = (0.0, 0.0, 0.0)

    @property
    def is_ca1_split_model(self) -> bool:
        return self.asset_machine_model == "ca1" and self.has_split_components

    @property
    def is_c1_split_model(self) -> bool:
        return self.asset_machine_model == "c1" and self.has_split_components

    @property
    def is_c1_model(self) -> bool:
        return self.machine_model == "c1" or self.is_c1_split_model

    @property
    def uses_bed_aligned_motion(self) -> bool:
        return self.is_ca1_split_model

    def scene_point(self, x: float, y: float, z: float) -> list[float]:
        if self.is_c1_model:
            return c1_model_point(x, y, z)
        if self.transform is None:
            return [x, y, z]
        if self.is_ca1_split_model:
            return ca1_bed_scene_point(self.transform, x, y, z)
        return self.transform.point(x, y, z)

    def spindle_face_point(self, x: float, y: float, z: float) -> list[float]:
        if self.is_c1_model:
            return c1_spindle_face_point(x, y, z)
        if self.is_ca1_split_model and self.transform is not None:
            return ca1_spindle_face_scene_point(self.transform, x, y, z)
        return self.scene_point(x, y, z)

    def active_envelope_point(self, x: float, y: float, z: float) -> list[float]:
        if self.is_c1_model:
            return c1_spindle_face_point(x, y, z)
        if self.is_ca1_split_model and self.transform is not None:
            return ca1_envelope_scene_point(self.transform, x, y, z)
        return self.scene_point(x, y, z)

    def bed_y_delta(self, raw_position: list[float], scene_position: list[float]) -> float:
        if self.uses_bed_aligned_motion and self.transform is not None:
            return -scene_position[1]
        return -raw_position[1]

    def atc_rack_tool_position(self, x: float, y: float, z: float, bed_y_delta: float) -> list[float]:
        if self.transform is None:
            return [0.0, 0.0, 0.0]

        rack_position = self.scene_point(x, y, z)
        if self.uses_bed_aligned_motion or self.is_c1_model:
            rack_position[1] += bed_y_delta
        if self.is_c1_model:
            rack_position[2] = C1_ATC_RACK_TOP_SCENE_Z
        return rack_position

    def spindle_marker_position(self, raw_position: list[float], scene_position: list[float]) -> list[float]:
        if not self.has_split_components:
            return scene_position

        if self.is_c1_split_model and self.transform is not None:
            spindle_face = c1_spindle_face_point(raw_position[0], raw_position[1], raw_position[2])
            return [spindle_face[0], self.model_offset[1] + self.spindle_face_local[1], spindle_face[2]]

        if self.uses_bed_aligned_motion and self.transform is not None:
            spindle_face = ca1_spindle_face_scene_point(self.transform, *raw_position)
            return [spindle_face[0], self.model_offset[1] + self.spindle_face_local[1], spindle_face[2]]

        return [
            self.model_offset[0] + self.spindle_face_local[0] + raw_position[0],
            self.model_offset[1] + self.spindle_face_local[1],
            self.model_offset[2] + self.spindle_face_local[2] + raw_position[2],
        ]

    def axis_component_positions(
        self, raw_position: list[float], scene_position: list[float]
    ) -> AxisComponentPositions:
        x_delta: float
        z_delta: float
        gantry_y_delta: float
        y_delta = self.bed_y_delta(raw_position, scene_position)
        offset_x, offset_y, offset_z = self.model_offset
        local_x, local_y, local_z = self.spindle_face_local

        if self.is_c1_split_model and self.transform is not None:
            spindle_face = c1_spindle_face_point(raw_position[0], raw_position[1], raw_position[2])
            x_delta = spindle_face[0] - offset_x - local_x
            gantry_y_delta = 0.0
            z_delta = spindle_face[2] - offset_z - local_z
            y_delta = offset_y + local_y - spindle_face[1]
        elif self.uses_bed_aligned_motion and self.transform is not None:
            spindle_face = ca1_spindle_face_scene_point(self.transform, *raw_position)
            x_delta = spindle_face[0] - offset_x - local_x
            z_delta = spindle_face[2] - offset_z - local_z
            gantry_y_delta = 0.0
        else:
            x_delta = raw_position[0]
            z_delta = raw_position[2]
            gantry_y_delta = 0.0

        bed_y_delta = y_delta + C1_BED_MESH_Y_ALIGNMENT_MM if self.is_c1_split_model else y_delta
        positions = {
            "base": (offset_x, offset_y, offset_z),
            "x": (offset_x + x_delta, offset_y + gantry_y_delta, offset_z),
            "z": (offset_x + x_delta, offset_y + gantry_y_delta, offset_z + z_delta),
            "y3": (offset_x, offset_y + bed_y_delta, offset_z),
            "y4": (offset_x, offset_y + bed_y_delta, offset_z),
            "a_chuck": (offset_x, offset_y + bed_y_delta, offset_z),
        }
        return AxisComponentPositions(bed_y_delta=y_delta, positions=positions)
