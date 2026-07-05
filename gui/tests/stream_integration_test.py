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

import os
import socket
import tempfile
import threading
from pathlib import Path
from typing import Any

import pytest

from gui.core.app_config import default_stream_simulator
from gui.protocol.sim_client import SimulatorClient

TEST_SD_CONFIG = "sd_ok true\nsoft_endstop.enable true\n"
pytestmark = pytest.mark.integration
STREAM_STARTUP_TIMEOUT_S = 45.0


def _wait_for_stream_startup(
    events: list[Any],
    snapshots: list[Any],
    io_events: list[Any],
    condition: threading.Condition,
) -> None:
    with condition:

        def ready() -> bool:
            return bool(
                snapshots
                and snapshots[-1].firmware_booted
                and snapshots[-1].homed
                and io_events
                and len(events) > 5
                and events[-1].axes
            )

        if ready():
            return
        condition.wait_for(
            ready,
            timeout=STREAM_STARTUP_TIMEOUT_S,
        )
        if ready():
            return

        latest = snapshots[-1] if snapshots else None
    if latest is None:
        pytest.fail(f"simulator stream did not publish a machine snapshot within {STREAM_STARTUP_TIMEOUT_S:.0f}s")
    pytest.fail(
        "simulator stream did not reach homed startup state within "
        f"{STREAM_STARTUP_TIMEOUT_S:.0f}s: "
        f"firmware_booted={latest.firmware_booted}, homed={latest.homed}, io_events={len(io_events)}"
    )


def test_stream_client_receives_live_simulator_state() -> None:
    root = Path(__file__).resolve().parents[2]
    configured_binary = os.environ.get("CARVERA_SIMULATOR_BINARY")
    simulator_binary = Path(configured_binary) if configured_binary else default_stream_simulator(root)
    if not simulator_binary.exists():
        pytest.skip(f"{simulator_binary} is not built")

    events = []
    snapshots = []
    io_events = []
    stream_condition = threading.Condition()

    def handle_event(event) -> None:
        with stream_condition:
            if event.WhichOneof("event") == "machine_telemetry":
                events.append(event.machine_telemetry)
            if event.WhichOneof("event") == "machine_snapshot":
                snapshots.append(event.machine_snapshot)
            if event.WhichOneof("event") == "physical_io":
                io_events.append(event.physical_io)
            stream_condition.notify_all()

    simulator = SimulatorClient(
        simulator_binary,
        stream_frames=True,
        event_handler=handle_event,
    )
    with tempfile.TemporaryDirectory(prefix="carvera_sim_gui_stream_test_") as tmp:
        sd = Path(tmp)
        (sd / "config.txt").write_text(TEST_SD_CONFIG, encoding="utf-8")
        simulator.start()
        try:
            simulator.mount_filesystem("sd", sd)
            simulator.set_machine_model("c1")
            simulator.set_realtime()
            transport = simulator.start_interactive_transport(enable_uart=True, tcp_ports=[0])
            assert transport.tcp_endpoints[0].host == "0.0.0.0"
            assert transport.tcp_endpoints[0].port > 0
            _wait_for_stream_startup(events, snapshots, io_events, stream_condition)
            with stream_condition:
                latest_snapshot = snapshots[-1]
                latest_event = events[-1]
                latest_io = io_events[-1]
                event_count = len(events)
            assert latest_snapshot.firmware_booted is True
            assert latest_snapshot.homed is True
            assert latest_snapshot.HasField("work_area")
            assert event_count > 5
            assert len(latest_event.axes) > 0
            assert latest_io.front_panel.power_rails.v24 is True
            endpoint = transport.tcp_endpoints[0]
            with socket.create_connection(("127.0.0.1", endpoint.port), timeout=5.0) as connection:
                connection.settimeout(5.0)
                connection.sendall(b"?\n")
                status = b""
                while b"MPos:" not in status:
                    chunk = connection.recv(4096)
                    assert chunk, "TCP transport closed before returning machine status"
                    status += chunk
        finally:
            simulator.stop()
