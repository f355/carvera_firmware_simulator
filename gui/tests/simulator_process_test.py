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

import sys
import time
from pathlib import Path

import pytest

from gui.protocol.simulator_process import SimulatorProcess


def test_start_does_not_allocate_a_windows_console(tmp_path: Path) -> None:
    if sys.platform != "win32":
        pytest.skip("Windows console behavior")

    result = tmp_path / "console.txt"
    probe = tmp_path / "probe.py"
    probe.write_text(
        "import ctypes, pathlib, sys\n"
        "pathlib.Path(sys.argv[1]).write_text(str(bool(ctypes.windll.kernel32.GetConsoleWindow())))\n"
    )
    launcher = tmp_path / "probe.cmd"
    launcher.write_text(f'@"{sys.executable}" "{probe}" "{result}"\n')

    process = SimulatorProcess(launcher)
    process.start()
    try:
        deadline = time.monotonic() + 5.0
        while not result.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        assert result.read_text() == "False"
    finally:
        process.stop()
