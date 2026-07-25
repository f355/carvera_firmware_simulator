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

from gui.core.transport_log import TransportLogFormatter, TransportLogStore, parse_transport_log_line


def _makera_frame(packet_type: int, payload: bytes) -> bytes:
    data_length = len(payload) + 3
    return b"\x86\x68" + data_length.to_bytes(2, "big") + bytes([packet_type]) + payload + b"\x00\x00\x55\xaa"


def _escaped(data: bytes) -> str:
    result = ""
    for byte in data:
        if byte == ord("\n"):
            result += r"\n"
        elif byte == ord("\r"):
            result += r"\r"
        elif byte == ord("\t"):
            result += r"\t"
        elif 0x20 <= byte < 0x7F:
            result += chr(byte)
        else:
            result += rf"\x{byte:02x}"
    return result


def test_parse_transport_log_line_supports_current_and_legacy_timestamps() -> None:
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


def test_transport_log_store_uses_non_consuming_cursors() -> None:
    entry = parse_transport_log_line("[sim wifi rx] M6 T1\\n")
    assert entry is not None
    store = TransportLogStore()
    store.append(entry)
    cursor, entries = store.cursor_and_entries_since(0)
    assert cursor == 1
    assert entries == [entry]
    assert store.cursor_and_entries_since(0) == (1, [entry])
    assert store.cursor_and_entries_since(cursor) == (1, [])


def test_transport_log_formatter_decodes_text_and_makera_frames() -> None:
    formatter = TransportLogFormatter()
    plain = parse_transport_log_line("[sim wifi rx] M6 T1\\n")
    assert plain is not None
    assert [entry.payload for entry in formatter.format_entry(plain)] == ["M6 T1"]

    frames = _makera_frame(0xA2, b"version") + _makera_frame(0x81, b"<Idle|MPos:1,2,3>\n")
    framed = parse_transport_log_line(f"[sim wifi tx] {_escaped(frames)}")
    assert framed is not None
    assert [entry.payload for entry in formatter.format_entry(framed)] == [
        "CTRL_MULTI  version",
        "STATUS  <Idle|MPos:1,2,3>",
    ]


def test_transport_log_formatter_reassembles_fragmented_frames() -> None:
    formatter = TransportLogFormatter()
    frame = _makera_frame(0xB0, b"download /sd/config.txt\n")
    first = parse_transport_log_line(f"[sim wifi rx] {_escaped(frame[:7])}")
    second = parse_transport_log_line(f"[sim wifi rx] {_escaped(frame[7:])}")
    assert first is not None
    assert second is not None
    assert formatter.format_entry(first) == []
    assert [entry.payload for entry in formatter.format_entry(second)] == ["FILE_START  download /sd/config.txt"]


def test_transport_log_formatter_summarizes_truncated_file_data() -> None:
    formatter = TransportLogFormatter()
    frame = _makera_frame(0xB3, b"\x00\x00\x00\x01" + bytes(8192))
    logged_payload = f"{_escaped(frame[:512])}...({len(frame)} bytes)"
    entry = parse_transport_log_line(f"[sim wifi tx] {logged_payload}")
    assert entry is not None
    assert [formatted.payload for formatted in formatter.format_entry(entry)] == ["FILE_DATA  8192 bytes"]
