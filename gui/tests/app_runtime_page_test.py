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
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

import pytest

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="spawns the GUI server with POSIX process semantics")


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def test_nicegui_page_serves_simulator_controls() -> None:
    root = Path(__file__).resolve().parents[2]
    port = free_port()
    env = os.environ.copy()
    env["PYTHONPATH"] = str(root)
    env.pop("PYTEST_CURRENT_TEST", None)
    process = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "gui.app",
            "--port",
            str(port),
            "--simulator",
            str(root / "build" / "carvera_sim_stream_stdio"),
        ],
        cwd=root,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        url = f"http://127.0.0.1:{port}/"
        health_url = f"http://127.0.0.1:{port}/healthz"
        html = ""
        second_html = ""
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            if process.poll() is not None:
                output = process.stdout.read() if process.stdout is not None else ""
                raise RuntimeError(f"GUI exited early with {process.returncode}:\n{output}")
            try:
                with urllib.request.urlopen(health_url, timeout=0.2) as response:
                    health = response.read().decode("utf-8")
                if "ok" not in health:
                    raise RuntimeError("GUI health endpoint did not answer ok")
                with urllib.request.urlopen(url, timeout=0.2) as response:
                    html = response.read().decode("utf-8")
                break
            except OSError:
                time.sleep(0.1)
        if not html:
            raise RuntimeError("GUI did not answer")
        with urllib.request.urlopen(url, timeout=1.0) as response:
            second_html = response.read().decode("utf-8")
        expected = (
            "Carvera Simulator",
            "sim-page",
            "Carvera (C1)",
            "Carvera Air (CA1)",
            "Show 3D Machine",
            "Open SD Card Folder",
            "Tool Table",
            "Inputs",
            "4th axis connected",
            "Outputs",
            "G53",
        )
        if any(text not in html for text in expected):
            raise RuntimeError("GUI root response did not include simulator UI elements")
        obsolete = (
            "Physical Inputs",
            "GPIO Watch",
            "Firmware Switch Outputs",
            "PWM Outputs",
            "State source",
            "Stopped",
        )
        if any(text in html for text in obsolete):
            raise RuntimeError("GUI root response included obsolete status panel labels")
        if "Simulator controls are already open" in second_html:
            raise RuntimeError("second GUI root response should not be blocked by a single-client lock")
        if "Power" not in second_html or "sim-page" not in second_html:
            raise RuntimeError("second GUI root response should build an independent view of the simulator controls")
    finally:
        process.terminate()
        try:
            process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3.0)
