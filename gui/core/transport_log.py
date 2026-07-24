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

import re
from dataclasses import dataclass
from threading import Lock


_ISO8601_TIMESTAMP = r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z"
_TRANSPORT_LOG_RE = re.compile(
    rf"^(?:(?P<prefix_timestamp>{_ISO8601_TIMESTAMP}) )?"
    r"\[sim (?P<channel>\w+) (?P<direction>rx|tx)\]"
    rf"(?: (?P<legacy_timestamp>{_ISO8601_TIMESTAMP}))?"
    r" (?P<payload>.*)$"
)
_TRUNCATED_PAYLOAD_RE = re.compile(r"^(?P<payload>.*)\.\.\.\((?P<count>\d+) bytes\)$")
_MAKERA_HEADER = b"\x86\x68"
_MAKERA_FOOTER = b"\x55\xaa"
_MAKERA_PACKET_NAMES = {
    0x81: "STATUS",
    0x82: "DIAGNOSTIC",
    0x83: "LOAD_INFO",
    0x84: "LOAD_FINISH",
    0x85: "LOAD_ERROR",
    0x90: "INFO",
    0xA1: "CTRL_SINGLE",
    0xA2: "CTRL_MULTI",
    0xB0: "FILE_START",
    0xB1: "FILE_MD5",
    0xB2: "FILE_VIEW",
    0xB3: "FILE_DATA",
    0xB4: "FILE_END",
    0xB5: "FILE_CANCEL",
    0xB6: "FILE_RETRY",
}
_TEXT_PACKET_TYPES = frozenset({0x81, 0x82, 0x83, 0x84, 0x85, 0x90, 0xA1, 0xA2, 0xB0, 0xB1, 0xB4, 0xB5, 0xB6})


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


def _decode_printable_bytes(payload: str) -> tuple[bytes, int | None]:
    truncated = _TRUNCATED_PAYLOAD_RE.match(payload)
    if truncated is not None:
        payload = truncated.group("payload")
        total_bytes = int(truncated.group("count"))
    else:
        total_bytes = None

    decoded = bytearray()
    offset = 0
    while offset < len(payload):
        if payload[offset] == "\\" and offset + 1 < len(payload):
            escape = payload[offset + 1]
            if escape == "x" and offset + 3 < len(payload):
                try:
                    decoded.append(int(payload[offset + 2 : offset + 4], 16))
                except ValueError:
                    pass
                else:
                    offset += 4
                    continue
            controls = {"n": ord("\n"), "r": ord("\r"), "t": ord("\t")}
            if escape in controls:
                decoded.append(controls[escape])
                offset += 2
                continue

        character = payload[offset]
        codepoint = ord(character)
        if codepoint <= 0xFF:
            decoded.append(codepoint)
        else:
            decoded.extend(character.encode())
        offset += 1
    return bytes(decoded), total_bytes


def _readable_text(data: bytes) -> str | None:
    text = data.decode(errors="replace").strip("\x00\r\n")
    if not text or not all(character.isprintable() or character in "\r\n\t" for character in text):
        return None
    return " · ".join(part for part in text.replace("\r\n", "\n").split("\n") if part)


def _formatted_entry(source: TransportLogEntry, payload: str) -> TransportLogEntry:
    return TransportLogEntry(
        channel=source.channel,
        direction=source.direction,
        payload=payload,
        timestamp=source.timestamp,
    )


class TransportLogFormatter:
    def __init__(self) -> None:
        self._pending: dict[tuple[str, str], bytes] = {}

    def format_entry(self, entry: TransportLogEntry) -> list[TransportLogEntry]:
        decoded, truncated_total = _decode_printable_bytes(entry.payload)
        key = (entry.channel, entry.direction)
        pending = self._pending.pop(key, b"")
        data = pending + decoded
        if truncated_total is not None:
            return self._format_data(entry, data, len(pending) + truncated_total)

        header = data.find(_MAKERA_HEADER)
        if pending or header >= 0:
            return self._format_data(entry, data, None)

        if data.endswith(_MAKERA_HEADER[:1]):
            self._pending[key] = data[-1:]
            data = data[:-1]
        return self._format_plain(entry, data)

    def _format_data(
        self,
        entry: TransportLogEntry,
        data: bytes,
        truncated_total: int | None,
    ) -> list[TransportLogEntry]:
        output: list[TransportLogEntry] = []
        key = (entry.channel, entry.direction)
        offset = 0

        while offset < len(data):
            header = data.find(_MAKERA_HEADER, offset)
            if header < 0:
                if truncated_total is None and data.endswith(_MAKERA_HEADER[:1]):
                    output.extend(self._format_plain(entry, data[offset:-1]))
                    self._pending[key] = data[-1:]
                else:
                    output.extend(self._format_plain(entry, data[offset:]))
                break
            if header > offset:
                output.extend(self._format_plain(entry, data[offset:header]))
            if len(data) - header < 5:
                if truncated_total is None:
                    self._pending[key] = data[header:]
                break

            data_length = int.from_bytes(data[header + 2 : header + 4], "big")
            if data_length < 3:
                output.extend(self._format_plain(entry, data[header : header + 1]))
                offset = header + 1
                continue

            frame_length = data_length + 6
            payload_length = data_length - 3
            available = len(data) - header
            if available < frame_length:
                if truncated_total is not None and truncated_total - header >= frame_length:
                    payload = data[header + 5 :]
                    output.append(self._format_frame(entry, data[header + 4], payload, payload_length))
                elif truncated_total is None:
                    self._pending[key] = data[header:]
                break

            frame = data[header : header + frame_length]
            if frame[-2:] != _MAKERA_FOOTER:
                output.extend(self._format_plain(entry, data[header : header + 1]))
                offset = header + 1
                continue
            payload = frame[5 : 5 + payload_length]
            output.append(self._format_frame(entry, frame[4], payload, payload_length))
            offset = header + frame_length

        return output

    @staticmethod
    def _format_plain(entry: TransportLogEntry, data: bytes) -> list[TransportLogEntry]:
        if not data:
            return []
        text = _readable_text(data)
        return [_formatted_entry(entry, text if text is not None else f"{len(data)} bytes")]

    @staticmethod
    def _format_frame(
        entry: TransportLogEntry,
        packet_type: int,
        payload: bytes,
        payload_length: int,
    ) -> TransportLogEntry:
        name = _MAKERA_PACKET_NAMES.get(packet_type, f"PACKET_0x{packet_type:02X}")
        detail: str | None
        if packet_type == 0xB3:
            detail = f"{max(0, payload_length - 4)} bytes"
        elif packet_type in _TEXT_PACKET_TYPES:
            detail = _readable_text(payload)
        else:
            detail = f"{payload_length} bytes" if payload_length else None
        return _formatted_entry(entry, f"{name}  {detail}" if detail else name)


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
