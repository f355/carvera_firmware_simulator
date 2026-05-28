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

from gui.views.gpio_tab import DEFAULT_STOCK_BOX


def test_gpio_tab_test() -> None:
    expected = {
        "min_x": -30.0,
        "min_y": -30.0,
        "min_z": -20.0,
        "max_x": 30.0,
        "max_y": 30.0,
        "max_z": -1.0,
    }
    if DEFAULT_STOCK_BOX != expected:
        raise SystemExit("stock-box defaults should stay explicit and stable")
