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
from typing import Any

import pytest

import gui.views.comms_log_tab as comms_log_tab
from gui.core.transport_log import TransportLogEntry
from gui.views.comms_log_tab import CommsLogTabView


class FakeUiElement:
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self.deleted = False

    def classes(self, *args: Any, **kwargs: Any) -> FakeUiElement:
        return self

    def __enter__(self) -> FakeUiElement:
        return self

    def __exit__(self, *args: Any) -> None:
        return None

    def delete(self) -> None:
        self.deleted = True


class FakeContainer(FakeUiElement):
    def clear(self) -> None:
        pass

    def run_method(self, *args: Any) -> None:
        pass


def make_entry(payload: str) -> TransportLogEntry:
    return TransportLogEntry(timestamp="", channel="wifi", direction="rx", payload=payload)


@pytest.fixture()
def fake_ui(monkeypatch: pytest.MonkeyPatch) -> list[FakeUiElement]:
    rows: list[FakeUiElement] = []

    def fake_element(*args: Any, **kwargs: Any) -> FakeUiElement:
        row = FakeUiElement()
        rows.append(row)
        return row

    monkeypatch.setattr(comms_log_tab.ui, "element", fake_element)
    monkeypatch.setattr(comms_log_tab.ui, "label", FakeUiElement)
    return rows


def test_append_prunes_oldest_rows_beyond_cap(fake_ui: list[FakeUiElement]) -> None:
    view = CommsLogTabView(container=FakeContainer(), autoscroll=SimpleNamespace(value=False), max_rows=3)
    for index in range(5):
        view.append(make_entry(f"line {index}"))

    assert len(view.rows) == 3
    assert [row.deleted for row in fake_ui] == [True, True, False, False, False]


def test_clear_forgets_tracked_rows(fake_ui: list[FakeUiElement]) -> None:
    view = CommsLogTabView(container=FakeContainer(), autoscroll=SimpleNamespace(value=False), max_rows=3)
    view.append(make_entry("line"))
    view.clear()

    assert view.rows == []
