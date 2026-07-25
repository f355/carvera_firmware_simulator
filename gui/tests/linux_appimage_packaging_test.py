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
from pathlib import Path

import pytest

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="inspects the Linux AppImage packaging scripts")

ROOT = Path(__file__).resolve().parents[2]
COMMON_SCRIPT = ROOT / "scripts" / "linux_appimage_common.sh"


def run_common(function: str, value: str) -> str:
    result = subprocess.run(
        ["bash", "-c", 'source "$1"; "$2" "$3"', "bash", str(COMMON_SCRIPT), function, value],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


@pytest.mark.parametrize(
    ("machine", "artifact_architecture"),
    (("aarch64", "arm64"), ("arm64", "arm64"), ("x86_64", "amd64"), ("amd64", "amd64")),
)
def test_linux_packaging_maps_machine_names_to_artifact_architectures(machine: str, artifact_architecture: str) -> None:
    assert run_common("linux_artifact_architecture", machine) == artifact_architecture


@pytest.mark.parametrize(("machine", "electron_architecture"), (("aarch64", "arm64"), ("x86_64", "x64")))
def test_linux_packaging_maps_native_machine_names_to_electron_architectures(
    machine: str, electron_architecture: str
) -> None:
    assert run_common("linux_electron_architecture", machine) == electron_architecture


@pytest.mark.parametrize(("machine", "file_pattern"), (("aarch64", "ARM aarch64"), ("x86_64", "x86-64")))
def test_linux_packaging_uses_file_architecture_names(machine: str, file_pattern: str) -> None:
    assert run_common("linux_file_machine_pattern", machine) == file_pattern
