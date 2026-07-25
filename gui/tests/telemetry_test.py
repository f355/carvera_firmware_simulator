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

from gui.core.telemetry import TelemetryBuffer


class FakeEvent:
    def __init__(self, event_name: str, payload: Any = None) -> None:
        self._event_name = event_name
        self.machine_telemetry = payload

    def WhichOneof(self, _name: str) -> str:
        return self._event_name


def make_buffer() -> TelemetryBuffer[dict[str, int]]:
    return TelemetryBuffer(lambda telemetry: {"seq": telemetry.seq})


def emit(buffer: TelemetryBuffer[dict[str, int]], sequence: int) -> None:
    buffer.handle_stream_event(FakeEvent("machine_telemetry", SimpleNamespace(seq=sequence)))


def test_telemetry_buffer_ignores_unrelated_events() -> None:
    buffer = make_buffer()
    assert buffer.latest_since(0) == (0, None)

    buffer.handle_stream_event(FakeEvent("log", SimpleNamespace(seq=1)))
    assert buffer.latest_since(0) == (0, None)


def test_latest_since_reports_only_the_newest_telemetry() -> None:
    buffer = make_buffer()
    emit(buffer, 1)
    emit(buffer, 2)

    cursor, latest = buffer.latest_since(0)
    assert latest == {"seq": 2}
    assert buffer.latest_since(cursor) == (cursor, None)


def test_latest_since_advances_each_consumer_cursor() -> None:
    buffer = make_buffer()
    emit(buffer, 3)

    cursor, latest = buffer.latest_since(0)
    assert latest == {"seq": 3}

    same_cursor = cursor
    cursor, latest = buffer.latest_since(cursor)
    assert (cursor, latest) == (same_cursor, None)

    emit(buffer, 4)
    cursor, latest = buffer.latest_since(cursor)
    assert latest == {"seq": 4}
