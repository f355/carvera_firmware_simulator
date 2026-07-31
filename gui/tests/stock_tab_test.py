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

import pytest

from gui.tests.fakes import FakeControl
from gui.views.stock_tab import StockTabView, default_stock_box, load_default_stock


def test_default_stock_is_a_disabled_plate_at_each_models_anchor_one() -> None:
    c1 = default_stock_box("c1")
    ca1 = default_stock_box("ca1")

    assert (c1["min_x"], c1["min_y"]) == (-359.665, -234.480)
    assert (ca1["min_x"], ca1["min_y"]) == (-288.669, -201.902)
    assert c1["min_z"] == pytest.approx(-147.5)
    assert ca1["min_z"] == pytest.approx(-137.0)
    for stock in (c1, ca1):
        assert stock["max_x"] - stock["min_x"] == pytest.approx(150.0)
        assert stock["max_y"] - stock["min_y"] == pytest.approx(150.0)
        assert stock["max_z"] - stock["min_z"] == pytest.approx(10.0)


def test_loading_model_defaults_updates_stock_dimensions_and_disables_it() -> None:
    controls = {"enabled": FakeControl(True)}
    controls.update({name: FakeControl(value) for name, value in default_stock_box("c1").items()})
    view = StockTabView(box_controls={"stock": controls})

    load_default_stock(view, "ca1")

    assert controls["enabled"].value is False
    for name, value in default_stock_box("ca1").items():
        assert controls[name].value == value
