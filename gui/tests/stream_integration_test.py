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

import tempfile
import time
from pathlib import Path
from typing import Any

import pytest

from gui.protocol.sim_client import SimulatorClient

TEST_SD_CONFIG = "sd_ok true\nsoft_endstop.enable true\n"
pytestmark = pytest.mark.integration


def _has_motion_delta(events: list[Any]) -> bool:
    if len(events) < 2 or len(events[0].axes) == 0 or len(events[-1].axes) == 0:
        return False
    return events[0].axes[0].physical_mm != events[-1].axes[0].physical_mm


def test_stream_integration_test() -> None:
    root = Path(__file__).resolve().parents[2]
    simulator_binary = root / "build" / "carvera_sim_stream_stdio"
    if not simulator_binary.exists():
        pytest.skip("build/carvera_sim_stream_stdio is not built")

    events = []
    snapshots = []
    io_events = []

    def handle_event(event) -> None:
        if event.WhichOneof("event") == "machine_telemetry":
            events.append(event.machine_telemetry)
        if event.WhichOneof("event") == "machine_snapshot":
            snapshots.append(event.machine_snapshot)
        if event.WhichOneof("event") == "physical_io":
            io_events.append(event.physical_io)

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
            assert transport.tcp_endpoints[0].host == "127.0.0.1"
            assert transport.tcp_endpoints[0].port > 0
            for _ in range(30):
                if snapshots and snapshots[-1].HasField("work_area") and io_events and _has_motion_delta(events):
                    break
                time.sleep(0.1)
            assert len(snapshots) >= 1
            assert snapshots[-1].firmware_booted is True
            assert snapshots[-1].HasField("work_area")
            assert len(events) > 5
            assert len(events[-1].axes) > 0
            assert _has_motion_delta(events)
            assert len(io_events) >= 1
            assert io_events[-1].front_panel.power_rails.v24 is True
        finally:
            simulator.stop()
