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
from pathlib import Path

import pytest

from gui.protocol.model import snapshot_to_state
from gui.protocol.sim_client import SimulatorClient

TEST_SD_CONFIG = "sd_ok true\nsoft_endstop.enable true\n"
pytestmark = pytest.mark.integration


def test_simulator_integration_test() -> None:
    root = Path(__file__).resolve().parents[2]
    simulator_binary = root / "build" / "carvera_sim_stdio"
    if not simulator_binary.exists():
        pytest.skip("build/carvera_sim_stdio is not built")

    simulator = SimulatorClient(simulator_binary)
    with tempfile.TemporaryDirectory(prefix="carvera_sim_gui_test_") as tmp:
        sd = Path(tmp)
        (sd / "config.txt").write_text(TEST_SD_CONFIG, encoding="utf-8")
        simulator.start()
        try:
            simulator.mount_filesystem("sd", sd)
            simulator.set_machine_model("c1")
            simulator.set_realtime()
            snapshot = simulator.machine_snapshot()
            state = snapshot_to_state(snapshot)
            assert state.firmware_booted is True
            assert state.homed is True
            simulator.set_stock_box((-8.0, -8.0, -2.0, 8.0, 8.0, -1.0), enabled=True)
            front_panel = simulator.get_front_panel_state()
            assert front_panel.power_rails.v12 is True
            assert front_panel.power_rails.v24 is True
            simulator.set_main_button_pressed(True)
            assert simulator.get_front_panel_state().main_button_pressed is True
            simulator.set_main_button_pressed(False)
            simulator.set_e_stop_pressed(True)
            assert simulator.get_front_panel_state().e_stop_pressed is True
            simulator.set_e_stop_pressed(False)
        finally:
            simulator.stop()
