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

from dataclasses import dataclass
import re
from threading import Lock


_ISO8601_TIMESTAMP = r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z"
_TRANSPORT_LOG_RE = re.compile(
    rf"^(?:(?P<prefix_timestamp>{_ISO8601_TIMESTAMP}) )?"
    r"\[sim (?P<channel>\w+) (?P<direction>rx|tx)\]"
    rf"(?: (?P<legacy_timestamp>{_ISO8601_TIMESTAMP}))?"
    r" (?P<payload>.*)$"
)


@dataclass(frozen=True)
class TransportLogEntry:
    channel: str
    direction: str
    payload: str
    timestamp: str | None = None


def parse_transport_log_line(line: str) -> TransportLogEntry | None:
    match = _TRANSPORT_LOG_RE.match(line.strip())
    if match is None:
        return None
    return TransportLogEntry(
        channel=match.group("channel"),
        direction=match.group("direction"),
        payload=match.group("payload"),
        timestamp=match.group("prefix_timestamp") or match.group("legacy_timestamp"),
    )


class TransportLogStore:
    def __init__(self, max_entries: int = 2000) -> None:
        self.max_entries = max_entries
        self._entries: list[TransportLogEntry] = []
        self._base_cursor = 0
        self._lock = Lock()

    def append(self, entry: TransportLogEntry) -> None:
        with self._lock:
            self._entries.append(entry)
            overflow = len(self._entries) - self.max_entries
            if overflow > 0:
                del self._entries[:overflow]
                self._base_cursor += overflow

    def entries_since(self, cursor: int) -> list[TransportLogEntry]:
        with self._lock:
            start = max(0, cursor - self._base_cursor)
            return list(self._entries[start:])

    def cursor_and_entries_since(self, cursor: int) -> tuple[int, list[TransportLogEntry]]:
        with self._lock:
            start = max(0, cursor - self._base_cursor)
            return self._base_cursor + len(self._entries), list(self._entries[start:])
