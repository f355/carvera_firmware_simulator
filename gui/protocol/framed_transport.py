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

import threading
import time
from collections.abc import Callable
from typing import Any

from gui.generated import carvera_sim_pb2 as pb
from gui.protocol.client_error import SimulatorClientError
from gui.protocol.simulator_process import SimulatorProcess


class FramedTransport:
    def __init__(
        self,
        process: SimulatorProcess,
        *,
        stream_frames: bool = False,
        event_handler: Callable[[Any], None] | None = None,
        request_timeout_s: float = 10.0,
    ) -> None:
        self.process = process
        self.stream_frames = stream_frames
        self.event_handler = event_handler
        self.request_timeout_s = request_timeout_s
        self._next_id = 1
        self._request_lock = threading.Lock()
        self._write_lock = threading.Lock()
        self._response_condition = threading.Condition()
        self._responses: dict[int, pb.Response] = {}
        self._reader_error: str | None = None
        self._reader_thread: threading.Thread | None = None

    @property
    def powered(self) -> bool:
        return self.process.powered

    def start(self) -> None:
        if self.powered:
            return
        self.process.start()
        self._responses.clear()
        self._reader_error = None
        if self.stream_frames:
            self._reader_thread = threading.Thread(target=self._read_stream_frames, daemon=True)
            self._reader_thread.start()

    def stop(self) -> None:
        with self._request_lock:
            self.process.stop()
            self._set_reader_error("simulator stopped")
            if self._reader_thread is not None and self._reader_thread is not threading.current_thread():
                self._reader_thread.join(timeout=1.0)
            self._reader_thread = None

    def request(self, request: pb.Request) -> pb.Response:
        with self._request_lock:
            if not self.powered:
                self.start()
            process = self.process.handle
            assert process.stdin is not None
            assert process.stdout is not None

            with self._write_lock:
                request.id = self._next_id
                self._next_id += 1
                payload = request.SerializeToString()
                process.stdin.write(len(payload).to_bytes(4, "little"))
                process.stdin.write(payload)
                process.stdin.flush()

            response = self._wait_for_response(request.id) if self.stream_frames else self._read_response()
        if not response.ok:
            raise SimulatorClientError(response.error or "simulator request failed")
        return response

    def _read_response(self) -> pb.Response:
        process = self.process.handle
        assert process.stdout is not None

        header = process.stdout.read(4)
        if len(header) != 4:
            raise SimulatorClientError(self.process.format_error("simulator closed stdout"))
        size = int.from_bytes(header, "little")
        data = process.stdout.read(size)
        if len(data) != size:
            raise SimulatorClientError(self.process.format_error("short simulator response"))
        response = pb.Response()
        response.ParseFromString(data)
        return response

    def _read_stream_frames(self) -> None:
        try:
            process = self.process.handle
        except SimulatorClientError:
            return
        if process.stdout is None:
            return

        try:
            while True:
                header = process.stdout.read(4)
                if len(header) != 4:
                    self._set_reader_error(self.process.format_error("simulator closed stdout"))
                    return
                size = int.from_bytes(header, "little")
                data = process.stdout.read(size)
                if len(data) != size:
                    self._set_reader_error(self.process.format_error("short simulator response"))
                    return

                frame = pb.StreamFrame()
                frame.ParseFromString(data)
                payload = frame.WhichOneof("payload")
                if payload == "response":
                    with self._response_condition:
                        self._responses[frame.response.id] = frame.response
                        self._response_condition.notify_all()
                elif payload == "event" and self.event_handler is not None:
                    self.event_handler(frame.event)
        except Exception as exc:
            self._set_reader_error(str(exc))

    def _set_reader_error(self, message: str) -> None:
        with self._response_condition:
            self._reader_error = message
            self._response_condition.notify_all()

    def _wait_for_response(self, request_id: int) -> pb.Response:
        deadline = time.monotonic() + self.request_timeout_s
        with self._response_condition:
            while True:
                response = self._responses.pop(request_id, None)
                if response is not None:
                    return response
                if self._reader_error is not None:
                    raise SimulatorClientError(self._reader_error)
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise SimulatorClientError(
                        f"simulator request {request_id} timed out after {self.request_timeout_s:.1f}s"
                    )
                self._response_condition.wait(remaining)
