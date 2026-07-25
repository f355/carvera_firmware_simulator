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

import math
from typing import Any

# Scene cylinders are Y-up; rotate about X to stand them upright on the bed.
TOOL_ROTATION_X = -math.pi / 2


def vertical_cylinder(scene: Any, *, radius: float, height: float, color: str, radial_segments: int = 32) -> Any:
    return (
        scene.cylinder(top_radius=radius, bottom_radius=radius, height=height, radial_segments=radial_segments)
        .material(color)
        .rotate(TOOL_ROTATION_X, 0.0, 0.0)
    )
