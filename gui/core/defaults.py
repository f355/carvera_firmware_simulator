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

from typing import Any


DEFAULT_SD_CONFIG = "# Carvera simulator SD config.\nsd_ok true\n"
TOOL_SHANK_INSERT_MM = 20.0
DEFAULT_C1_TOOLS: list[dict[str, Any]] = [
    {
        "pocket": 0,
        "label": "Z probe",
        "tool": 0,
        "length_mm": 50.0,
        "occupied": True,
        "rack": True,
        "locked": True,
        "probe_tip_diameter_mm": 1.6,
    },
    {"pocket": 1, "tool": 1, "length_mm": 52.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
    {"pocket": 2, "tool": 2, "length_mm": 57.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
    {"pocket": 3, "tool": 3, "length_mm": 61.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
    {"pocket": 4, "tool": 4, "length_mm": 65.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
    {"pocket": 5, "tool": 5, "length_mm": 70.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
    {"pocket": 6, "tool": 6, "length_mm": 74.0, "occupied": True, "rack": True, "probe_tip_diameter_mm": 0.0},
]
DEFAULT_LOOSE_TOOLS: list[dict[str, Any]] = [
    {
        "pocket": 999999,
        "label": "3D probe",
        "tool": 999999,
        "length_mm": 70.0,
        "occupied": True,
        "rack": False,
        "locked": True,
        "probe_tip_diameter_mm": 4.0,
    },
]
MACHINE_COMPONENT_COLORS = {
    "base": "#d9dee4",
    "x": "#d9dee4",
    "z": "#d9dee4",
    "y3": "#d9dee4",
    "y4": "#d9dee4",
    "a_chuck": "#c7cdd4",
    "shell": "#d9dee4",
}
# Local to the transformed split-model Z-axis components. These anchors are
# the physical spindle face, not a firmware coordinate.
CA1_SPINDLE_FACE_LOCAL = (58.597, 12.939, 49.0)
C1_SPINDLE_FACE_LOCAL = (-29.954, -19.092, 66.625)
PIN_WATCHES = [
    ("X max", 0, 25),
    ("Y max", 1, 4),
    ("Z max", 1, 8),
    ("Probe", 2, 6),
    ("ETS", 0, 5),
    ("ATC detector", 0, 20),
    ("ATC home", 1, 0),
    ("Spindle PWM", 2, 5),
    ("Laser PWM", 2, 4),
    ("Work light", 2, 0),
    ("Vacuum", 2, 13),
    ("Probe charger", 0, 23),
]
PWM_WATCHES = [
    ("Spindle", 2, 5),
    ("Laser", 2, 4),
    ("Vacuum PWM", 2, 3),
    ("Spindle fan", 2, 1),
    ("Power fan", 2, 3),
    ("Extend PWM", 2, 2),
]
SWITCH_WATCHES = [
    ("light", "Light"),
    ("vacuum", "Vacuum"),
    ("spindle_fan", "Spindle fan"),
    ("power_fan", "Power fan"),
    ("tool_sensor", "Tool sensor"),
    ("probe_charger", "Probe charger"),
    ("air", "Air"),
    ("beep", "Beep"),
]
