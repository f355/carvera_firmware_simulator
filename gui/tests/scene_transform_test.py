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

from gui.core.defaults import C1_SPINDLE_FACE_LOCAL, CA1_SPINDLE_FACE_LOCAL
from gui.protocol.model import Box3D
from gui.scene.scene_geometry import MachineSceneGeometry
from gui.scene.scene_transform import (
    C1_ATC_RACK_TOP_SCENE_Z,
    C1_BED_MESH_Y_ALIGNMENT_MM,
    C1_HOME_SWITCH_SCENE_XYZ,
    CA1_BED_SURFACE_SCENE_Z,
    CA1_ETS_SCENE_XY,
    CA1_HOME_SWITCH_SCENE_XY,
    SceneTransform,
    c1_model_point,
    c1_spindle_face_point,
    ca1_bed_scene_point,
    ca1_envelope_scene_point,
)


def test_scene_transform_centers_xy_and_places_z_on_bed() -> None:
    work_area = Box3D(min_x=-371.0, min_y=-250.0, min_z=-135.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
    transform = SceneTransform.from_work_area(work_area)

    assert transform.point(-186.0, -125.5, -135.0) == [0.0, 0.0, 0.0]
    assert transform.point(-371.0, -250.0, -135.0) == [-185.0, -124.5, 0.0]
    assert transform.point(-1.0, -1.0, -1.0) == [185.0, 124.5, 134.0]


def test_ca1_landmarks_align_with_physical_travel() -> None:
    ca1_physical_travel = Box3D(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
    ca1_transform = SceneTransform.from_work_area(ca1_physical_travel)
    ca1_ets = ca1_bed_scene_point(ca1_transform, -11.0, -7.0, ca1_transform.bed_z)
    assert CA1_HOME_SWITCH_SCENE_XY == (153.016, 107.096)
    assert ca1_ets[:2] == list(CA1_ETS_SCENE_XY)

    ca1_bed_bottom = ca1_bed_scene_point(ca1_transform, -11.0, -7.0, ca1_transform.bed_z)
    assert ca1_bed_bottom == [
        CA1_ETS_SCENE_XY[0],
        CA1_ETS_SCENE_XY[1],
        CA1_BED_SURFACE_SCENE_Z,
    ]
    assert ca1_envelope_scene_point(ca1_transform, -2.0, -2.0, -2.0)[2] - CA1_BED_SURFACE_SCENE_Z == 135.0
    assert ca1_envelope_scene_point(ca1_transform, -302.0, -212.0, -121.0)[2] - CA1_BED_SURFACE_SCENE_Z == 16.0


def test_c1_landmarks_align_with_machine_model() -> None:
    c1_physical_travel = Box3D(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
    rack_pickup_x = -4.158
    front_pocket_y = -234.568
    tool_setter_y = -54.568
    toolrack_z = -112.5
    rack_midline_x = 172.5
    bed_front_y = -39.5
    bed_back_y = 200.5

    assert C1_HOME_SWITCH_SCENE_XYZ == (176.658, 225.068, 160.5)

    rack_midline = c1_model_point(rack_pickup_x, front_pocket_y, toolrack_z)
    tool_setter = c1_model_point(rack_pickup_x, tool_setter_y, toolrack_z)
    hard_limit_far_top_right = c1_model_point(
        c1_physical_travel.max_x,
        c1_physical_travel.max_y,
        c1_physical_travel.max_z,
    )

    assert round(rack_midline[0], 3) == rack_midline_x
    assert round(rack_midline[1] - bed_front_y, 3) == 30.0
    assert round(bed_back_y - tool_setter[1], 3) == 30.0
    assert round(C1_ATC_RACK_TOP_SCENE_Z - rack_midline[2], 3) == 15.0
    assert hard_limit_far_top_right == [177.658, 226.068, 161.5]
    assert c1_spindle_face_point(
        c1_physical_travel.max_x,
        c1_physical_travel.max_y,
        c1_physical_travel.max_z,
    ) == [177.658, 226.068, 179.5]


def test_machine_scene_geometry_mapper_keeps_view_placement_math_pure() -> None:
    ca1_physical_travel = Box3D(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
    ca1_transform = SceneTransform.from_work_area(ca1_physical_travel)
    ca1_geometry = MachineSceneGeometry(
        machine_model="ca1",
        asset_machine_model="ca1",
        has_split_components=True,
        transform=ca1_transform,
        model_offset=(-82.5, -13.5, 33.0),
        spindle_face_local=CA1_SPINDLE_FACE_LOCAL,
    )

    raw_position = [-2.0, -2.0, -2.0]
    scene_position = ca1_transform.point(*raw_position)
    spindle = ca1_geometry.spindle_marker_position(raw_position, scene_position)
    assert [round(value, 3) for value in spindle] == [
        round(ca1_geometry.spindle_face_point(*raw_position)[0], 3),
        -0.561,
        round(ca1_geometry.spindle_face_point(*raw_position)[2], 3),
    ]
    assert ca1_geometry.bed_y_delta(raw_position, scene_position) == -scene_position[1]

    c1_transform = SceneTransform.from_work_area(
        Box3D(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
    )
    c1_geometry = MachineSceneGeometry(
        machine_model="c1",
        asset_machine_model="c1",
        has_split_components=True,
        transform=c1_transform,
        model_offset=(-141.5, 13.0, 86.0),
        spindle_face_local=C1_SPINDLE_FACE_LOCAL,
    )
    c1_scene_position = c1_transform.point(*raw_position)
    component_positions = c1_geometry.axis_component_positions(raw_position, c1_scene_position)

    assert [round(value, 3) for value in c1_geometry.spindle_marker_position(raw_position, c1_scene_position)] == [
        round(c1_spindle_face_point(*raw_position)[0], 3),
        -6.092,
        round(c1_spindle_face_point(*raw_position)[2], 3),
    ]
    assert round(component_positions.bed_y_delta, 3) == round(-6.092 - c1_spindle_face_point(*raw_position)[1], 3)
    assert round(component_positions.positions["y3"][1], 3) == round(
        13.0 + component_positions.bed_y_delta + C1_BED_MESH_Y_ALIGNMENT_MM,
        3,
    )
    assert c1_geometry.atc_rack_tool_position(-4.158, -234.568, -112.5, component_positions.bed_y_delta) == [
        172.5,
        -9.5 + component_positions.bed_y_delta,
        C1_ATC_RACK_TOP_SCENE_Z,
    ]
