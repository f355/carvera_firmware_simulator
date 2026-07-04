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

import importlib
from pathlib import Path
from typing import Any


def load_runtime_module() -> Any:
    return importlib.import_module("gui.core.webview_runtime")


def test_bundled_webview_settings_select_fixed_runtime_on_windows(tmp_path: Path) -> None:
    runtime = tmp_path / "webview2"
    runtime.mkdir()
    (runtime / "msedgewebview2.exe").touch()

    settings = load_runtime_module().bundled_webview_settings(tmp_path, platform="win32")

    assert settings == {"WEBVIEW2_RUNTIME_PATH": str(runtime)}


def test_bundled_webview_settings_ignore_missing_or_non_windows_runtime(tmp_path: Path) -> None:
    runtime = tmp_path / "webview2"
    runtime.mkdir()
    (runtime / "msedgewebview2.exe").touch()
    module = load_runtime_module()

    assert module.bundled_webview_settings(tmp_path, platform="linux") == {}
    assert module.bundled_webview_settings(tmp_path / "missing", platform="win32") == {}


def test_prepare_fixed_runtime_grants_windows_app_container_read_access(tmp_path: Path) -> None:
    calls: list[tuple[list[str], bool, int, Any, Any]] = []
    (tmp_path / "msedgewebview2.exe").touch()

    def run(command: list[str], *, check: bool, creationflags: int, stdout: Any, stderr: Any) -> None:
        calls.append((command, check, creationflags, stdout, stderr))

    load_runtime_module().prepare_fixed_runtime(tmp_path, platform="win32", run=run, creationflags=123)

    assert calls == [
        (
            ["icacls", str(tmp_path), "/grant", "*S-1-15-2-2:(OI)(CI)(RX)"],
            False,
            123,
            load_runtime_module().subprocess.DEVNULL,
            load_runtime_module().subprocess.DEVNULL,
        ),
        (
            ["icacls", str(tmp_path), "/grant", "*S-1-15-2-1:(OI)(CI)(RX)"],
            False,
            123,
            load_runtime_module().subprocess.DEVNULL,
            load_runtime_module().subprocess.DEVNULL,
        ),
    ]


def test_configure_bundled_runtime_sets_webview2_loader_override(tmp_path: Path) -> None:
    runtime = tmp_path / "webview2"
    runtime.mkdir()
    (runtime / "msedgewebview2.exe").touch()
    environ: dict[str, str] = {}

    selected = load_runtime_module().configure_bundled_webview_runtime(
        tmp_path,
        environ=environ,
        platform="win32",
    )

    assert selected == runtime
    assert environ == {"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER": str(runtime)}
