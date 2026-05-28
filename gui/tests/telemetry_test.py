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


def test_telemetry_test() -> None:
    buffer = TelemetryBuffer(lambda telemetry: {"seq": telemetry.seq})
    if buffer.take_latest() is not None:
        raise SystemExit("empty telemetry buffer should return None")

    buffer.handle_stream_event(FakeEvent("log", SimpleNamespace(seq=1)))
    if buffer.take_latest() is not None:
        raise SystemExit("non-telemetry stream events should be ignored")

    buffer.handle_stream_event(FakeEvent("machine_telemetry", SimpleNamespace(seq=1)))
    buffer.handle_stream_event(FakeEvent("machine_telemetry", SimpleNamespace(seq=2)))
    latest = buffer.take_latest()
    if latest != {"seq": 2}:
        raise SystemExit("telemetry buffer should drain to the latest event")
    if buffer.take_latest() is not None:
        raise SystemExit("take_latest should drain buffered telemetry")

    buffer.handle_stream_event(FakeEvent("machine_telemetry", SimpleNamespace(seq=3)))
    if buffer.latest() != {"seq": 3}:
        raise SystemExit("latest should expose telemetry without consuming it")
    if buffer.latest() != {"seq": 3}:
        raise SystemExit("latest should be reusable by multiple GUI clients")

    cursor = 0
    cursor, latest = buffer.latest_since(cursor)
    if latest != {"seq": 3}:
        raise SystemExit("latest_since should return the newest telemetry for a fresh cursor")
    same_cursor = cursor
    cursor, latest = buffer.latest_since(cursor)
    if cursor != same_cursor or latest is not None:
        raise SystemExit("latest_since should not replay stale telemetry to one GUI view")

    buffer.handle_stream_event(FakeEvent("machine_telemetry", SimpleNamespace(seq=4)))
    cursor, latest = buffer.latest_since(cursor)
    if latest != {"seq": 4}:
        raise SystemExit("latest_since should return newly arrived telemetry")
