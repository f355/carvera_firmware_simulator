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

import subprocess
import sys
import threading
from collections import deque
from collections.abc import Callable
from pathlib import Path

from gui.core.logging import log_gui_event
from gui.protocol.client_error import SimulatorClientError


class SimulatorProcess:
    def __init__(
        self,
        binary: Path,
        *,
        stderr_handler: Callable[[str], None] | None = None,
        inherit_stderr: bool = False,
    ) -> None:
        self.binary = Path(binary)
        self.stderr_handler = stderr_handler
        self.inherit_stderr = inherit_stderr
        self._process: subprocess.Popen[bytes] | None = None
        self._stderr_thread: threading.Thread | None = None
        self._stderr_lock = threading.Lock()
        self._stderr_lines: deque[str] = deque(maxlen=40)

    @property
    def powered(self) -> bool:
        return self._process is not None and self._process.poll() is None

    @property
    def handle(self) -> subprocess.Popen[bytes]:
        if self._process is None:
            raise SimulatorClientError("simulator process is not running")
        return self._process

    def start(self) -> None:
        if self.powered:
            return
        if not self.binary.exists():
            raise SimulatorClientError(f"simulator binary not found: {self.binary}")
        log_gui_event(f"launching simulator binary path={self.binary}")
        process = subprocess.Popen(
            [str(self.binary)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0,
        )
        self._process = process
        log_gui_event(f"simulator process started pid={process.pid}")
        with self._stderr_lock:
            self._stderr_lines.clear()
        self._stderr_thread = threading.Thread(target=self._read_stderr_lines, args=(process,), daemon=True)
        self._stderr_thread.start()

    def stop(self) -> None:
        process = self._process
        self._process = None
        if process is None:
            return
        log_gui_event(f"stopping simulator process pid={process.pid}")
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                log_gui_event(f"killing unresponsive simulator process pid={process.pid}")
                process.kill()
                process.wait(timeout=2.0)
        log_gui_event(f"simulator process stopped pid={process.pid} returncode={process.returncode}")
        if self._stderr_thread is not None and self._stderr_thread is not threading.current_thread():
            self._stderr_thread.join(timeout=1.0)
        self._stderr_thread = None

    def format_error(self, message: str) -> str:
        with self._stderr_lock:
            stderr = "\n".join(self._stderr_lines)
        return f"{message}: {stderr.strip()}" if stderr.strip() else message

    def _read_stderr_lines(self, process: subprocess.Popen[bytes]) -> None:
        if process.stderr is None:
            return
        try:
            for raw_line in iter(process.stderr.readline, b""):
                line = raw_line.decode("utf-8", errors="replace").rstrip("\n")
                with self._stderr_lock:
                    self._stderr_lines.append(line)
                if self.inherit_stderr:
                    print(line, file=sys.stderr)
                if self.stderr_handler is not None:
                    try:
                        self.stderr_handler(line)
                    except Exception as exc:
                        log_gui_event(f"stderr handler failed on line {line!r}: {exc}")
        except Exception as exc:
            log_gui_event(f"stderr reader stopped: {exc}")
