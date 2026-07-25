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
from pathlib import Path

import pytest

from gui.core.app_config import default_stream_simulator

REPO_ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


def _built_or_skip(binary: Path) -> Path:
    if not binary.exists():
        pytest.skip(f"{binary} is not built")
    return binary


@pytest.fixture()
def stdio_api_binary() -> Path:
    configured = os.environ.get("CARVERA_SIMULATOR_API_BINARY")
    return _built_or_skip(Path(configured) if configured else REPO_ROOT / "build" / "carvera_sim_stdio")


@pytest.fixture()
def stream_stdio_binary() -> Path:
    configured = os.environ.get("CARVERA_SIMULATOR_BINARY")
    return _built_or_skip(Path(configured) if configured else default_stream_simulator(REPO_ROOT))
