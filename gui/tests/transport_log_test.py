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

from gui.core.transport_log import TransportLogStore, parse_transport_log_line


def test_transport_log_test() -> None:
    entry = parse_transport_log_line("[sim wifi rx] M6 T1\\n")
    assert entry is not None
    assert entry.channel == "wifi"
    assert entry.direction == "rx"
    assert entry.timestamp is None
    assert entry.payload == "M6 T1\\n"

    stamped = parse_transport_log_line("2026-06-02T10:42:31.123Z [sim uart tx] ok\\n")
    assert stamped is not None
    assert stamped.timestamp == "2026-06-02T10:42:31.123Z"
    assert stamped.payload == "ok\\n"

    legacy = parse_transport_log_line("[sim uart tx] 2026-06-02T10:42:31.123Z ok\\n")
    assert legacy is not None
    assert legacy.timestamp == "2026-06-02T10:42:31.123Z"

    assert parse_transport_log_line("NiceGUI ready") is None

    store = TransportLogStore()
    store.append(entry)
    assert store.entries_since(0) == [entry]
    assert store.entries_since(0) == [entry]
    cursor, entries = store.cursor_and_entries_since(0)
    assert cursor == 1
    assert entries == [entry]
    assert store.cursor_and_entries_since(cursor) == (1, [])
