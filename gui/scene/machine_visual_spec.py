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

"""Calibrated per-machine rendering constants.

Every value here was measured against the bundled GLB exports (produced by
scripts/split_machine_models.py from one specific CAD Assistant export) using
scripts/model_tools/analyze_glb_geometry.py, which replays the GUI transform
(scale 1000, rotate X 90 degrees, model offset). Re-exporting or re-splitting
the models invalidates these values together with the scene anchors in
gui/scene/scene_transform.py - regenerate them, do not tweak them by eye.
"""

from __future__ import annotations

from dataclasses import dataclass

from .scene_transform import C1_ATC_RACK_TOP_SCENE_Z, CA1_ETS_TOP_SCENE_Z

Vector3 = tuple[float, float, float]

# Local to the transformed split-model Z-axis components. These anchors are
# the physical spindle face, not a firmware coordinate.
CA1_SPINDLE_FACE_LOCAL: Vector3 = (58.597, 12.939, 49.0)
C1_SPINDLE_FACE_LOCAL: Vector3 = (-29.954, -19.092, 66.625)

TOOL_SETTER_BUTTON_RADIUS_MM = 4.0
TOOL_SETTER_TRIGGER_BELOW_TOP_MM = 1.0
_C1_TOOL_SETTER_BUTTON_HEIGHT_MM = 8.0
_CA1_TOOL_SETTER_BUTTON_HEIGHT_MM = 3.0


@dataclass(frozen=True)
class MachineVisualSpec:
    # CAD Assistant exports the models in meters; the offset places the bed on
    # the simulator bed plane after the upright rotation.
    model_offset: Vector3
    model_rotation_degrees: Vector3
    spindle_face_local: Vector3
    tool_setter_button_height_mm: float
    # Visual pin for the tool-setter button top; deliberately disagrees with
    # the firmware trigger Z because the GLBs omit pocket depth.
    tool_setter_scene_top_z: float
    spindle_max_rpm: float


C1_VISUAL_SPEC = MachineVisualSpec(
    model_offset=(-141.5, 13.0, 86.0),
    model_rotation_degrees=(90.0, 0.0, 0.0),
    spindle_face_local=C1_SPINDLE_FACE_LOCAL,
    tool_setter_button_height_mm=_C1_TOOL_SETTER_BUTTON_HEIGHT_MM,
    tool_setter_scene_top_z=C1_ATC_RACK_TOP_SCENE_Z + _C1_TOOL_SETTER_BUTTON_HEIGHT_MM,
    spindle_max_rpm=15_500.0,
)

CA1_VISUAL_SPEC = MachineVisualSpec(
    model_offset=(-82.5, -13.5, 33.0),
    model_rotation_degrees=(90.0, 0.0, 0.0),
    spindle_face_local=CA1_SPINDLE_FACE_LOCAL,
    tool_setter_button_height_mm=_CA1_TOOL_SETTER_BUTTON_HEIGHT_MM,
    tool_setter_scene_top_z=CA1_ETS_TOP_SCENE_Z + TOOL_SETTER_TRIGGER_BELOW_TOP_MM,
    spindle_max_rpm=14_500.0,
)

VISUAL_SPECS = {"c1": C1_VISUAL_SPEC, "ca1": CA1_VISUAL_SPEC}
