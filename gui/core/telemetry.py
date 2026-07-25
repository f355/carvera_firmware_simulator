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

from collections.abc import Callable
from threading import Lock
from typing import Any, Generic, TypeVar

T = TypeVar("T")


class TelemetryBuffer(Generic[T]):
    def __init__(self, convert: Callable[[Any], T], *, event_name: str = "machine_telemetry") -> None:
        self._convert = convert
        self._event_name = event_name
        self._latest: T | None = None
        self._version = 0
        self._lock = Lock()

    def handle_stream_event(self, event: Any) -> None:
        if event.WhichOneof("event") == self._event_name:
            telemetry = self._convert(getattr(event, self._event_name))
            with self._lock:
                self._latest = telemetry
                self._version += 1

    def latest_since(self, cursor: int) -> tuple[int, T | None]:
        with self._lock:
            if cursor == self._version:
                return cursor, None
            return self._version, self._latest
