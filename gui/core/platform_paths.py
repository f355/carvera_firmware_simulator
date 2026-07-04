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
from collections.abc import Callable, Mapping
from pathlib import Path
from typing import Any


def user_data_root(
    *,
    platform: str | None = None,
    home: Path | None = None,
    env: Mapping[str, str] | None = None,
) -> Path:
    platform = sys.platform if platform is None else platform
    home = Path.home() if home is None else home
    env = os.environ if env is None else env

    if platform == "darwin":
        return home / "Library" / "Application Support" / "Carvera Simulator"
    if platform == "win32":
        local_app_data = env.get("LOCALAPPDATA")
        root = Path(local_app_data) if local_app_data else home / "AppData" / "Local"
        return root / "Carvera Simulator"
    xdg_data_home = env.get("XDG_DATA_HOME")
    root = Path(xdg_data_home) if xdg_data_home else home / ".local" / "share"
    return root / "carvera-simulator"


def open_in_file_manager(
    path: Path,
    *,
    platform: str | None = None,
    run: Callable[..., Any] = subprocess.run,
) -> None:
    platform = sys.platform if platform is None else platform
    path.mkdir(parents=True, exist_ok=True)
    if platform == "darwin":
        command = ["open", str(path)]
    elif platform == "win32":
        command = ["explorer", str(path)]
    else:
        command = ["xdg-open", str(path)]
    run(command, check=True)
