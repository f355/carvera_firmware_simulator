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

from gui.core.defaults import (
    DEFAULT_C1_TOOLS,
    DEFAULT_LOOSE_TOOLS,
    DEFAULT_SD_CONFIG,
    PIN_WATCHES,
    PWM_WATCHES,
    SWITCH_WATCHES,
)


def test_defaults_test() -> None:
    if "sd_ok true" not in DEFAULT_SD_CONFIG:
        raise SystemExit("default SD config should mark the simulated card present")
    if len(DEFAULT_C1_TOOLS) != 7:
        raise SystemExit("C1 tool rack should expose probe pocket plus six physical tool pockets")
    if DEFAULT_C1_TOOLS[0]["tool"] != 0 or not DEFAULT_C1_TOOLS[0]["locked"]:
        raise SystemExit("C1 defaults should include the stock probe in pocket 0")
    if not all(tool["occupied"] for tool in DEFAULT_C1_TOOLS):
        raise SystemExit("default C1 tools should be loaded in the rack")
    if DEFAULT_LOOSE_TOOLS[0]["tool"] < 999990 or not DEFAULT_LOOSE_TOOLS[0]["locked"]:
        raise SystemExit("loose default tools should include a non-editable 3D probe")
    for label, port, pin in PIN_WATCHES + PWM_WATCHES:
        if not label or not (0 <= port <= 4) or not (0 <= pin <= 31):
            raise SystemExit(f"invalid watch pin: {label} {port}.{pin}")
    if "beep" not in {name for name, _ in SWITCH_WATCHES}:
        raise SystemExit("switch watches should include beep")
