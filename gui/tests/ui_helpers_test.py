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

from types import SimpleNamespace

import pytest

from gui.tests.fakes import FakeControl, FakeLabel
from gui.views.ui_helpers import event_bool, event_value, set_badge, set_control_locked, set_status_badge


def test_badge_helpers_replace_text_and_status_classes() -> None:
    label = FakeLabel()
    set_badge(label, True, "HIT", "clear", warn=True)
    assert label.text == "HIT"
    assert label.classes_text == "badge-warn"

    set_badge(label, False, "HIT", "clear", warn=True)
    assert label.text == "clear"
    assert label.classes_text == "badge-off"

    set_status_badge(label, True, "booted", "off")
    assert label.text == "booted"
    assert label.classes_text == "badge-on"

    set_status_badge(label, False, "booted", "off")
    assert label.text == "off"
    assert label.classes_text == "badge-warn"


def test_control_locking_toggles_disabled_state() -> None:
    control = FakeControl()
    set_control_locked(control, True)
    assert control.disabled

    set_control_locked(control, False)
    assert not control.disabled


@pytest.mark.parametrize(("value", "expected"), [("false", False), ("true", True), (0, False), (1, True)])
def test_event_bool_coerces_control_values(value: object, expected: bool) -> None:
    assert event_bool(SimpleNamespace(value=value)) is expected


@pytest.mark.parametrize(
    ("event", "expected"),
    [
        (SimpleNamespace(value=6.0), 6.0),
        (SimpleNamespace(args=[7.0]), 7.0),
        (SimpleNamespace(args=8.0), 8.0),
    ],
)
def test_event_value_accepts_nicegui_event_shapes(event: SimpleNamespace, expected: float) -> None:
    assert event_value(event) == expected
