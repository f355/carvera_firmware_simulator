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

from gui.views.styles import SIM_CSS


def test_styles_test() -> None:
    required_selectors = [
        ".sim-page",
        ".main-splitter",
        ".machine-scene",
        ".side-panel",
        ".tool-table",
        ".plain-number",
        ".temperature-drive",
    ]
    missing = [selector for selector in required_selectors if selector not in SIM_CSS]
    if missing:
        raise SystemExit(f"missing stylesheet selectors: {', '.join(missing)}")
    if "resize: horizontal" in SIM_CSS:
        raise SystemExit("side panel should use the splitter handle instead of CSS resize")
