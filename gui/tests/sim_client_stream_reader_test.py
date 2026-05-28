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

import stat
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

from gui.protocol.sim_client import SimulatorClient, SimulatorClientError, pb


def write_fake_stream_server(path: Path, repo_root: Path) -> None:
    script = f"""#!{sys.executable}
import sys
import threading
import time

sys.path.insert(0, {str(repo_root / "gui" / "generated")!r})
import carvera_sim_pb2 as pb


def write_message(message):
    data = message.SerializeToString()
    sys.stdout.buffer.write(len(data).to_bytes(4, "little"))
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def send_event():
    time.sleep(0.05)
    frame = pb.StreamFrame()
    frame.event.sequence = 1
    frame.event.machine_telemetry.homed = True
    write_message(frame)


threading.Thread(target=send_event, daemon=True).start()

while True:
    header = sys.stdin.buffer.read(4)
    if len(header) != 4:
        break
    size = int.from_bytes(header, "little")
    data = sys.stdin.buffer.read(size)
    request = pb.Request()
    request.ParseFromString(data)

    frame = pb.StreamFrame()
    frame.response.id = request.id
    frame.response.ok = True
    frame.response.status.time_us = 123
    write_message(frame)
"""
    path.write_text(script, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def write_silent_stream_server(path: Path, repo_root: Path) -> None:
    script = f"""#!{sys.executable}
import sys
import time

sys.path.insert(0, {str(repo_root / "gui" / "generated")!r})
import carvera_sim_pb2 as pb

header = sys.stdin.buffer.read(4)
if len(header) == 4:
    size = int.from_bytes(header, "little")
    sys.stdin.buffer.read(size)
    time.sleep(5.0)
"""
    path.write_text(script, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def test_sim_client_stream_reader_test() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    events: list[pb.Event] = []

    with tempfile.TemporaryDirectory(prefix="carvera_sim_stream_client_test_") as tmp:
        server = Path(tmp) / "fake_stream_server.py"
        write_fake_stream_server(server, repo_root)

        client = SimulatorClient(server, stream_frames=True, event_handler=events.append)
        client.start()
        try:
            deadline = time.monotonic() + 1.0
            while time.monotonic() < deadline and not events:
                time.sleep(0.01)
            assert events, "stream-mode client did not dispatch events while idle"
            assert events[0].WhichOneof("event") == "machine_telemetry"

            request = pb.Request()
            request.get_status.SetInParent()
            response = client.request(request)
            assert response.ok is True
            assert response.id == 1
            assert response.status.time_us == 123
        finally:
            client.stop()


def test_sim_client_logs_process_lifecycle(capsys: Any) -> None:
    repo_root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="carvera_sim_stream_client_log_test_") as tmp:
        server = Path(tmp) / "fake_stream_server.py"
        write_fake_stream_server(server, repo_root)

        client = SimulatorClient(server, stream_frames=True)
        client.start()
        client.stop()

    stderr = capsys.readouterr().err
    assert "[sim gui] launching simulator binary" in stderr
    assert "[sim gui] simulator process stopped" in stderr


def test_sim_client_stream_request_timeout() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="carvera_sim_stream_timeout_test_") as tmp:
        server = Path(tmp) / "silent_stream_server.py"
        write_silent_stream_server(server, repo_root)

        client = SimulatorClient(server, stream_frames=True, request_timeout_s=0.1)
        client.start()
        try:
            request = pb.Request()
            request.get_status.SetInParent()
            try:
                client.request(request)
            except SimulatorClientError as exc:
                assert "timed out" in str(exc)
            else:
                raise AssertionError("stream-mode client should time out when the simulator does not answer")
        finally:
            client.stop()
