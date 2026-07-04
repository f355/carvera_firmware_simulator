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
import subprocess
import sys
from collections.abc import Callable, MutableMapping
from pathlib import Path
from typing import Any


WEBVIEW2_RUNTIME_DIRECTORY = "webview2"
WEBVIEW2_EXECUTABLE = "msedgewebview2.exe"
APP_CONTAINER_SIDS = ("*S-1-15-2-2", "*S-1-15-2-1")


def bundled_webview_settings(resource_root: Path, *, platform: str | None = None) -> dict[str, str]:
    platform = sys.platform if platform is None else platform
    runtime = resource_root / WEBVIEW2_RUNTIME_DIRECTORY
    if platform != "win32" or not (runtime / WEBVIEW2_EXECUTABLE).is_file():
        return {}
    return {"WEBVIEW2_RUNTIME_PATH": str(runtime)}


def configure_bundled_webview_runtime(
    resource_root: Path,
    *,
    environ: MutableMapping[str, str] | None = None,
    platform: str | None = None,
) -> Path | None:
    """Select the fixed runtime through WebView2's inherited loader override."""
    settings = bundled_webview_settings(resource_root, platform=platform)
    runtime = settings.get("WEBVIEW2_RUNTIME_PATH")
    if runtime is None:
        return None
    environ = os.environ if environ is None else environ
    environ["WEBVIEW2_BROWSER_EXECUTABLE_FOLDER"] = runtime
    return Path(runtime)


def prepare_fixed_runtime(
    runtime: Path,
    *,
    platform: str | None = None,
    run: Callable[..., Any] = subprocess.run,
    creationflags: int | None = None,
) -> None:
    platform = sys.platform if platform is None else platform
    if platform != "win32" or not (runtime / WEBVIEW2_EXECUTABLE).is_file():
        return
    if creationflags is None:
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    for sid in APP_CONTAINER_SIDS:
        run(
            ["icacls", str(runtime), "/grant", f"{sid}:(OI)(CI)(RX)"],
            check=False,
            creationflags=creationflags,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
