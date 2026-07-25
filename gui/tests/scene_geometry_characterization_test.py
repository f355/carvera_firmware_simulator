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

"""Characterization pins for the firmware-to-scene coordinate mapping.

The expected values are captured from the calibrated mapping and must never
change beyond 1e-9 mm (far below the float32 precision the browser renders
at): the machine visuals were tuned against them. If a refactor trips one of
these assertions, the refactor is wrong, not the test.
"""

from __future__ import annotations

from typing import Any

import pytest

from gui.protocol.model import Box3D
from gui.scene.scene_geometry import MachineSceneGeometry
from gui.scene.scene_transform import SceneTransform

CA1_BOX = Box3D(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
C1_BOX = Box3D(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
C1_OFFSET = (-141.5, 13.0, 86.0)
CA1_OFFSET = (-82.5, -13.5, 33.0)
C1_LOCAL = (-29.954, -19.092, 66.625)
CA1_LOCAL = (58.597, 12.939, 49.0)

CONFIGS = {
    "c1_split": ("c1", "c1", True, C1_BOX, C1_OFFSET, C1_LOCAL),
    "c1_split_no_transform": ("c1", "c1", True, None, C1_OFFSET, C1_LOCAL),
    "ca1_split": ("ca1", "ca1", True, CA1_BOX, CA1_OFFSET, CA1_LOCAL),
    "ca1_split_no_transform": ("ca1", "ca1", True, None, CA1_OFFSET, CA1_LOCAL),
    "c1_fallback": ("c1", None, False, C1_BOX, C1_OFFSET, C1_LOCAL),
    "ca1_fallback": ("ca1", None, False, CA1_BOX, CA1_OFFSET, CA1_LOCAL),
    "ca1_fallback_no_transform": ("ca1", None, False, None, CA1_OFFSET, CA1_LOCAL),
    "unknown_fallback": (None, None, False, CA1_BOX, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
}

EXPECTED = {
    ("c1_split", (-2.0, -2.0, -2.0)): {
        "scene_point": [174.658, 223.068, 158.5],
        "spindle_face_point": [174.658, 223.068, 176.5],
        "active_envelope_point": [174.658, 223.068, 176.5],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [174.658, 228.568, 63.0],
        "spindle_marker_position": [174.658, -6.091999999999999, 176.5],
        "component_bed_y_delta": -229.16000000000003,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (204.61200000000002, 13.0, 86.0),
            "z": (204.61200000000002, 13.0, 109.875),
            "y3": (-141.5, -202.46000000000004, 86.0),
            "y4": (-141.5, -202.46000000000004, 86.0),
            "a_chuck": (-141.5, -202.46000000000004, 86.0),
        },
    },
    ("c1_split", (0.0, 0.0, 0.0)): {
        "scene_point": [176.658, 225.068, 160.5],
        "spindle_face_point": [176.658, 225.068, 178.5],
        "active_envelope_point": [176.658, 225.068, 178.5],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [176.658, 230.568, 63.0],
        "spindle_marker_position": [176.658, -6.091999999999999, 178.5],
        "component_bed_y_delta": -231.16000000000003,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (206.61200000000002, 13.0, 86.0),
            "z": (206.61200000000002, 13.0, 111.875),
            "y3": (-141.5, -204.46000000000004, 86.0),
            "y4": (-141.5, -204.46000000000004, 86.0),
            "a_chuck": (-141.5, -204.46000000000004, 86.0),
        },
    },
    ("c1_split", (-150.25, -100.5, -37.125)): {
        "scene_point": [26.407999999999987, 124.56800000000001, 123.375],
        "spindle_face_point": [26.407999999999987, 124.56800000000001, 141.375],
        "active_envelope_point": [26.407999999999987, 124.56800000000001, 141.375],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [26.407999999999987, 130.068, 63.0],
        "spindle_marker_position": [26.407999999999987, -6.091999999999999, 141.375],
        "component_bed_y_delta": -130.66000000000003,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (56.361999999999995, 13.0, 86.0),
            "z": (56.361999999999995, 13.0, 74.75),
            "y3": (-141.5, -103.96000000000002, 86.0),
            "y4": (-141.5, -103.96000000000002, 86.0),
            "a_chuck": (-141.5, -103.96000000000002, 86.0),
        },
    },
    ("c1_split", (12.5, 3.25, -1.75)): {
        "scene_point": [189.158, 228.318, 158.75],
        "spindle_face_point": [189.158, 228.318, 176.75],
        "active_envelope_point": [189.158, 228.318, 176.75],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [189.158, 233.818, 63.0],
        "spindle_marker_position": [189.158, -6.091999999999999, 176.75],
        "component_bed_y_delta": -234.41000000000003,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (219.11200000000002, 13.0, 86.0),
            "z": (219.11200000000002, 13.0, 110.125),
            "y3": (-141.5, -207.71000000000004, 86.0),
            "y4": (-141.5, -207.71000000000004, 86.0),
            "a_chuck": (-141.5, -207.71000000000004, 86.0),
        },
    },
    ("c1_split_no_transform", (-2.0, -2.0, -2.0)): {
        "scene_point": [174.658, 223.068, 158.5],
        "spindle_face_point": [174.658, 223.068, 176.5],
        "active_envelope_point": [174.658, 223.068, 176.5],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-173.454, -6.091999999999999, 150.625],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-143.5, 13.0, 86.0),
            "z": (-143.5, 13.0, 84.0),
            "y3": (-141.5, 28.7, 86.0),
            "y4": (-141.5, 28.7, 86.0),
            "a_chuck": (-141.5, 28.7, 86.0),
        },
    },
    ("c1_split_no_transform", (0.0, 0.0, 0.0)): {
        "scene_point": [176.658, 225.068, 160.5],
        "spindle_face_point": [176.658, 225.068, 178.5],
        "active_envelope_point": [176.658, 225.068, 178.5],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-171.454, -6.091999999999999, 152.625],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-141.5, 13.0, 86.0),
            "z": (-141.5, 13.0, 86.0),
            "y3": (-141.5, 26.7, 86.0),
            "y4": (-141.5, 26.7, 86.0),
            "a_chuck": (-141.5, 26.7, 86.0),
        },
    },
    ("c1_split_no_transform", (-150.25, -100.5, -37.125)): {
        "scene_point": [26.407999999999987, 124.56800000000001, 123.375],
        "spindle_face_point": [26.407999999999987, 124.56800000000001, 141.375],
        "active_envelope_point": [26.407999999999987, 124.56800000000001, 141.375],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-321.704, -6.091999999999999, 115.5],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-291.75, 13.0, 86.0),
            "z": (-291.75, 13.0, 48.875),
            "y3": (-141.5, 127.2, 86.0),
            "y4": (-141.5, 127.2, 86.0),
            "a_chuck": (-141.5, 127.2, 86.0),
        },
    },
    ("c1_split_no_transform", (12.5, 3.25, -1.75)): {
        "scene_point": [189.158, 228.318, 158.75],
        "spindle_face_point": [189.158, 228.318, 176.75],
        "active_envelope_point": [189.158, 228.318, 176.75],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-158.954, -6.091999999999999, 150.875],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-129.0, 13.0, 86.0),
            "z": (-129.0, 13.0, 84.25),
            "y3": (-141.5, 23.45, 86.0),
            "y4": (-141.5, 23.45, 86.0),
            "a_chuck": (-141.5, 23.45, 86.0),
        },
    },
    ("ca1_split", (-2.0, -2.0, -2.0)): {
        "scene_point": [151.016, 105.096, 98.0],
        "spindle_face_point": [150.0, 105.0, 113.0],
        "active_envelope_point": [151.016, 105.096, 113.0],
        "bed_y_delta": -105.0,
        "atc_rack_tool_position": [151.016, 110.596, 98.0],
        "spindle_marker_position": [150.0, -0.5609999999999999, 113.0],
        "component_bed_y_delta": -105.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (91.40299999999999, -13.5, 33.0),
            "z": (91.40299999999999, -13.5, 64.0),
            "y3": (-82.5, -118.5, 33.0),
            "y4": (-82.5, -118.5, 33.0),
            "a_chuck": (-82.5, -118.5, 33.0),
        },
    },
    ("ca1_split", (0.0, 0.0, 0.0)): {
        "scene_point": [153.016, 107.096, 100.0],
        "spindle_face_point": [152.0, 107.0, 115.0],
        "active_envelope_point": [153.016, 107.096, 115.0],
        "bed_y_delta": -107.0,
        "atc_rack_tool_position": [153.016, 112.596, 100.0],
        "spindle_marker_position": [152.0, -0.5609999999999999, 115.0],
        "component_bed_y_delta": -107.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (93.40299999999999, -13.5, 33.0),
            "z": (93.40299999999999, -13.5, 66.0),
            "y3": (-82.5, -120.5, 33.0),
            "y4": (-82.5, -120.5, 33.0),
            "a_chuck": (-82.5, -120.5, 33.0),
        },
    },
    ("ca1_split", (-150.25, -100.5, -37.125)): {
        "scene_point": [2.765999999999991, 6.596000000000004, 62.875],
        "spindle_face_point": [1.75, 6.5, 77.875],
        "active_envelope_point": [2.765999999999991, 6.596000000000004, 77.875],
        "bed_y_delta": -6.5,
        "atc_rack_tool_position": [2.765999999999991, 12.096000000000004, 62.875],
        "spindle_marker_position": [1.75, -0.5609999999999999, 77.875],
        "component_bed_y_delta": -6.5,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-56.847, -13.5, 33.0),
            "z": (-56.847, -13.5, 28.875),
            "y3": (-82.5, -20.0, 33.0),
            "y4": (-82.5, -20.0, 33.0),
            "a_chuck": (-82.5, -20.0, 33.0),
        },
    },
    ("ca1_split", (12.5, 3.25, -1.75)): {
        "scene_point": [165.516, 110.346, 98.25],
        "spindle_face_point": [164.5, 110.25, 113.25],
        "active_envelope_point": [165.516, 110.346, 113.25],
        "bed_y_delta": -110.25,
        "atc_rack_tool_position": [165.516, 115.846, 98.25],
        "spindle_marker_position": [164.5, -0.5609999999999999, 113.25],
        "component_bed_y_delta": -110.25,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (105.90299999999999, -13.5, 33.0),
            "z": (105.90299999999999, -13.5, 64.25),
            "y3": (-82.5, -123.75, 33.0),
            "y4": (-82.5, -123.75, 33.0),
            "a_chuck": (-82.5, -123.75, 33.0),
        },
    },
    ("ca1_split_no_transform", (-2.0, -2.0, -2.0)): {
        "scene_point": [-2.0, -2.0, -2.0],
        "spindle_face_point": [-2.0, -2.0, -2.0],
        "active_envelope_point": [-2.0, -2.0, -2.0],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-25.903, -0.5609999999999999, 80.0],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-84.5, -13.5, 33.0),
            "z": (-84.5, -13.5, 31.0),
            "y3": (-82.5, -11.5, 33.0),
            "y4": (-82.5, -11.5, 33.0),
            "a_chuck": (-82.5, -11.5, 33.0),
        },
    },
    ("ca1_split_no_transform", (0.0, 0.0, 0.0)): {
        "scene_point": [0.0, 0.0, 0.0],
        "spindle_face_point": [0.0, 0.0, 0.0],
        "active_envelope_point": [0.0, 0.0, 0.0],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-23.903, -0.5609999999999999, 82.0],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-82.5, -13.5, 33.0),
            "z": (-82.5, -13.5, 33.0),
            "y3": (-82.5, -13.5, 33.0),
            "y4": (-82.5, -13.5, 33.0),
            "a_chuck": (-82.5, -13.5, 33.0),
        },
    },
    ("ca1_split_no_transform", (-150.25, -100.5, -37.125)): {
        "scene_point": [-150.25, -100.5, -37.125],
        "spindle_face_point": [-150.25, -100.5, -37.125],
        "active_envelope_point": [-150.25, -100.5, -37.125],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-174.153, -0.5609999999999999, 44.875],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-232.75, -13.5, 33.0),
            "z": (-232.75, -13.5, -4.125),
            "y3": (-82.5, 87.0, 33.0),
            "y4": (-82.5, 87.0, 33.0),
            "a_chuck": (-82.5, 87.0, 33.0),
        },
    },
    ("ca1_split_no_transform", (12.5, 3.25, -1.75)): {
        "scene_point": [12.5, 3.25, -1.75],
        "spindle_face_point": [12.5, 3.25, -1.75],
        "active_envelope_point": [12.5, 3.25, -1.75],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-11.402999999999999, -0.5609999999999999, 80.25],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-70.0, -13.5, 33.0),
            "z": (-70.0, -13.5, 31.25),
            "y3": (-82.5, -16.75, 33.0),
            "y4": (-82.5, -16.75, 33.0),
            "a_chuck": (-82.5, -16.75, 33.0),
        },
    },
    ("c1_fallback", (-2.0, -2.0, -2.0)): {
        "scene_point": [174.658, 223.068, 158.5],
        "spindle_face_point": [174.658, 223.068, 176.5],
        "active_envelope_point": [174.658, 223.068, 176.5],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [174.658, 228.568, 63.0],
        "spindle_marker_position": [183.5, 123.0, 134.0],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-143.5, 13.0, 86.0),
            "z": (-143.5, 13.0, 84.0),
            "y3": (-141.5, 15.0, 86.0),
            "y4": (-141.5, 15.0, 86.0),
            "a_chuck": (-141.5, 15.0, 86.0),
        },
    },
    ("c1_fallback", (0.0, 0.0, 0.0)): {
        "scene_point": [176.658, 225.068, 160.5],
        "spindle_face_point": [176.658, 225.068, 178.5],
        "active_envelope_point": [176.658, 225.068, 178.5],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [176.658, 230.568, 63.0],
        "spindle_marker_position": [185.5, 125.0, 136.0],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-141.5, 13.0, 86.0),
            "z": (-141.5, 13.0, 86.0),
            "y3": (-141.5, 13.0, 86.0),
            "y4": (-141.5, 13.0, 86.0),
            "a_chuck": (-141.5, 13.0, 86.0),
        },
    },
    ("c1_fallback", (-150.25, -100.5, -37.125)): {
        "scene_point": [26.407999999999987, 124.56800000000001, 123.375],
        "spindle_face_point": [26.407999999999987, 124.56800000000001, 141.375],
        "active_envelope_point": [26.407999999999987, 124.56800000000001, 141.375],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [26.407999999999987, 130.068, 63.0],
        "spindle_marker_position": [35.25, 24.5, 98.875],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-291.75, 13.0, 86.0),
            "z": (-291.75, 13.0, 48.875),
            "y3": (-141.5, 113.5, 86.0),
            "y4": (-141.5, 113.5, 86.0),
            "a_chuck": (-141.5, 113.5, 86.0),
        },
    },
    ("c1_fallback", (12.5, 3.25, -1.75)): {
        "scene_point": [189.158, 228.318, 158.75],
        "spindle_face_point": [189.158, 228.318, 176.75],
        "active_envelope_point": [189.158, 228.318, 176.75],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [189.158, 233.818, 63.0],
        "spindle_marker_position": [198.0, 128.25, 134.25],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (-141.5, 13.0, 86.0),
            "x": (-129.0, 13.0, 86.0),
            "z": (-129.0, 13.0, 84.25),
            "y3": (-141.5, 9.75, 86.0),
            "y4": (-141.5, 9.75, 86.0),
            "a_chuck": (-141.5, 9.75, 86.0),
        },
    },
    ("ca1_fallback", (-2.0, -2.0, -2.0)): {
        "scene_point": [150.0, 105.0, 120.0],
        "spindle_face_point": [150.0, 105.0, 120.0],
        "active_envelope_point": [150.0, 105.0, 120.0],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [150.0, 105.0, 120.0],
        "spindle_marker_position": [150.0, 105.0, 120.0],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-84.5, -13.5, 33.0),
            "z": (-84.5, -13.5, 31.0),
            "y3": (-82.5, -11.5, 33.0),
            "y4": (-82.5, -11.5, 33.0),
            "a_chuck": (-82.5, -11.5, 33.0),
        },
    },
    ("ca1_fallback", (0.0, 0.0, 0.0)): {
        "scene_point": [152.0, 107.0, 122.0],
        "spindle_face_point": [152.0, 107.0, 122.0],
        "active_envelope_point": [152.0, 107.0, 122.0],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [152.0, 107.0, 122.0],
        "spindle_marker_position": [152.0, 107.0, 122.0],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-82.5, -13.5, 33.0),
            "z": (-82.5, -13.5, 33.0),
            "y3": (-82.5, -13.5, 33.0),
            "y4": (-82.5, -13.5, 33.0),
            "a_chuck": (-82.5, -13.5, 33.0),
        },
    },
    ("ca1_fallback", (-150.25, -100.5, -37.125)): {
        "scene_point": [1.75, 6.5, 84.875],
        "spindle_face_point": [1.75, 6.5, 84.875],
        "active_envelope_point": [1.75, 6.5, 84.875],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [1.75, 6.5, 84.875],
        "spindle_marker_position": [1.75, 6.5, 84.875],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-232.75, -13.5, 33.0),
            "z": (-232.75, -13.5, -4.125),
            "y3": (-82.5, 87.0, 33.0),
            "y4": (-82.5, 87.0, 33.0),
            "a_chuck": (-82.5, 87.0, 33.0),
        },
    },
    ("ca1_fallback", (12.5, 3.25, -1.75)): {
        "scene_point": [164.5, 110.25, 120.25],
        "spindle_face_point": [164.5, 110.25, 120.25],
        "active_envelope_point": [164.5, 110.25, 120.25],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [164.5, 110.25, 120.25],
        "spindle_marker_position": [164.5, 110.25, 120.25],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-70.0, -13.5, 33.0),
            "z": (-70.0, -13.5, 31.25),
            "y3": (-82.5, -16.75, 33.0),
            "y4": (-82.5, -16.75, 33.0),
            "a_chuck": (-82.5, -16.75, 33.0),
        },
    },
    ("ca1_fallback_no_transform", (-2.0, -2.0, -2.0)): {
        "scene_point": [-2.0, -2.0, -2.0],
        "spindle_face_point": [-2.0, -2.0, -2.0],
        "active_envelope_point": [-2.0, -2.0, -2.0],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-2.0, -2.0, -2.0],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-84.5, -13.5, 33.0),
            "z": (-84.5, -13.5, 31.0),
            "y3": (-82.5, -11.5, 33.0),
            "y4": (-82.5, -11.5, 33.0),
            "a_chuck": (-82.5, -11.5, 33.0),
        },
    },
    ("ca1_fallback_no_transform", (0.0, 0.0, 0.0)): {
        "scene_point": [0.0, 0.0, 0.0],
        "spindle_face_point": [0.0, 0.0, 0.0],
        "active_envelope_point": [0.0, 0.0, 0.0],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [0.0, 0.0, 0.0],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-82.5, -13.5, 33.0),
            "z": (-82.5, -13.5, 33.0),
            "y3": (-82.5, -13.5, 33.0),
            "y4": (-82.5, -13.5, 33.0),
            "a_chuck": (-82.5, -13.5, 33.0),
        },
    },
    ("ca1_fallback_no_transform", (-150.25, -100.5, -37.125)): {
        "scene_point": [-150.25, -100.5, -37.125],
        "spindle_face_point": [-150.25, -100.5, -37.125],
        "active_envelope_point": [-150.25, -100.5, -37.125],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [-150.25, -100.5, -37.125],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-232.75, -13.5, 33.0),
            "z": (-232.75, -13.5, -4.125),
            "y3": (-82.5, 87.0, 33.0),
            "y4": (-82.5, 87.0, 33.0),
            "a_chuck": (-82.5, 87.0, 33.0),
        },
    },
    ("ca1_fallback_no_transform", (12.5, 3.25, -1.75)): {
        "scene_point": [12.5, 3.25, -1.75],
        "spindle_face_point": [12.5, 3.25, -1.75],
        "active_envelope_point": [12.5, 3.25, -1.75],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [0.0, 0.0, 0.0],
        "spindle_marker_position": [12.5, 3.25, -1.75],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (-82.5, -13.5, 33.0),
            "x": (-70.0, -13.5, 33.0),
            "z": (-70.0, -13.5, 31.25),
            "y3": (-82.5, -16.75, 33.0),
            "y4": (-82.5, -16.75, 33.0),
            "a_chuck": (-82.5, -16.75, 33.0),
        },
    },
    ("unknown_fallback", (-2.0, -2.0, -2.0)): {
        "scene_point": [150.0, 105.0, 120.0],
        "spindle_face_point": [150.0, 105.0, 120.0],
        "active_envelope_point": [150.0, 105.0, 120.0],
        "bed_y_delta": 2.0,
        "atc_rack_tool_position": [150.0, 105.0, 120.0],
        "spindle_marker_position": [150.0, 105.0, 120.0],
        "component_bed_y_delta": 2.0,
        "component_positions": {
            "base": (0.0, 0.0, 0.0),
            "x": (-2.0, 0.0, 0.0),
            "z": (-2.0, 0.0, -2.0),
            "y3": (0.0, 2.0, 0.0),
            "y4": (0.0, 2.0, 0.0),
            "a_chuck": (0.0, 2.0, 0.0),
        },
    },
    ("unknown_fallback", (0.0, 0.0, 0.0)): {
        "scene_point": [152.0, 107.0, 122.0],
        "spindle_face_point": [152.0, 107.0, 122.0],
        "active_envelope_point": [152.0, 107.0, 122.0],
        "bed_y_delta": -0.0,
        "atc_rack_tool_position": [152.0, 107.0, 122.0],
        "spindle_marker_position": [152.0, 107.0, 122.0],
        "component_bed_y_delta": -0.0,
        "component_positions": {
            "base": (0.0, 0.0, 0.0),
            "x": (0.0, 0.0, 0.0),
            "z": (0.0, 0.0, 0.0),
            "y3": (0.0, 0.0, 0.0),
            "y4": (0.0, 0.0, 0.0),
            "a_chuck": (0.0, 0.0, 0.0),
        },
    },
    ("unknown_fallback", (-150.25, -100.5, -37.125)): {
        "scene_point": [1.75, 6.5, 84.875],
        "spindle_face_point": [1.75, 6.5, 84.875],
        "active_envelope_point": [1.75, 6.5, 84.875],
        "bed_y_delta": 100.5,
        "atc_rack_tool_position": [1.75, 6.5, 84.875],
        "spindle_marker_position": [1.75, 6.5, 84.875],
        "component_bed_y_delta": 100.5,
        "component_positions": {
            "base": (0.0, 0.0, 0.0),
            "x": (-150.25, 0.0, 0.0),
            "z": (-150.25, 0.0, -37.125),
            "y3": (0.0, 100.5, 0.0),
            "y4": (0.0, 100.5, 0.0),
            "a_chuck": (0.0, 100.5, 0.0),
        },
    },
    ("unknown_fallback", (12.5, 3.25, -1.75)): {
        "scene_point": [164.5, 110.25, 120.25],
        "spindle_face_point": [164.5, 110.25, 120.25],
        "active_envelope_point": [164.5, 110.25, 120.25],
        "bed_y_delta": -3.25,
        "atc_rack_tool_position": [164.5, 110.25, 120.25],
        "spindle_marker_position": [164.5, 110.25, 120.25],
        "component_bed_y_delta": -3.25,
        "component_positions": {
            "base": (0.0, 0.0, 0.0),
            "x": (12.5, 0.0, 0.0),
            "z": (12.5, 0.0, -1.75),
            "y3": (0.0, -3.25, 0.0),
            "y4": (0.0, -3.25, 0.0),
            "a_chuck": (0.0, -3.25, 0.0),
        },
    },
}


@pytest.mark.parametrize(("config_name", "point"), sorted(EXPECTED))
def test_mapping_is_bit_identical_to_the_calibrated_output(config_name: str, point: tuple[float, float, float]) -> None:
    machine_model, asset_model, has_split, box, offset, local = CONFIGS[config_name]
    transform = SceneTransform.from_work_area(box) if box is not None else None
    geometry = MachineSceneGeometry(
        machine_model=machine_model,
        asset_machine_model=asset_model,
        has_split_components=has_split,
        transform=transform,
        model_offset=offset,
        spindle_face_local=local,
    )
    raw = list(point)
    scene_pos = transform.point(*raw) if transform is not None else raw
    expected = EXPECTED[(config_name, point)]
    components = geometry.axis_component_positions(raw, scene_pos)

    assert_close(geometry.scene_point(*raw), expected["scene_point"])
    assert_close(geometry.spindle_face_point(*raw), expected["spindle_face_point"])
    assert_close(geometry.active_envelope_point(*raw), expected["active_envelope_point"])
    assert_close(geometry.bed_y_delta(raw, scene_pos), expected["bed_y_delta"])
    assert_close(geometry.atc_rack_tool_position(raw[0], raw[1], raw[2], 5.5), expected["atc_rack_tool_position"])
    assert_close(geometry.spindle_marker_position(raw, scene_pos), expected["spindle_marker_position"])
    assert_close(components.bed_y_delta, expected["component_bed_y_delta"])
    assert_close(components.positions, expected["component_positions"])


def assert_close(actual: Any, expected: Any) -> None:
    if isinstance(expected, dict):
        assert set(actual) == set(expected)
        for key, value in expected.items():
            assert_close(actual[key], value)
    elif isinstance(expected, (list, tuple)):
        assert len(actual) == len(expected)
        for actual_item, expected_item in zip(actual, expected, strict=True):
            assert_close(actual_item, expected_item)
    else:
        assert actual == pytest.approx(expected, abs=1e-9)
