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
import shutil
import subprocess
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
COMMON_SCRIPT = ROOT / "scripts" / "linux_appimage_common.sh"
APP_RUN = ROOT / "packaging" / "linux" / "AppRun"


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


@pytest.mark.parametrize(("machine", "appimage_architecture"), (("aarch64", "aarch64"), ("x86_64", "x86_64")))
def test_linux_packaging_maps_native_machine_names_to_appimage_architectures(
    machine: str, appimage_architecture: str
) -> None:
    assert run_common("linux_appimage_architecture", machine) == appimage_architecture


@pytest.mark.parametrize(("machine", "file_pattern"), (("aarch64", "ARM aarch64"), ("x86_64", "x86-64")))
def test_linux_packaging_uses_file_architecture_names(machine: str, file_pattern: str) -> None:
    assert run_common("linux_file_machine_pattern", machine) == file_pattern


def test_linux_builder_copies_a_system_library_omitted_by_pyinstaller(tmp_path: Path) -> None:
    system_library = tmp_path / "system" / "liblcms2.so.2.0.16"
    system_library.parent.mkdir()
    system_library.write_bytes(b"lcms2")

    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    ldconfig = bin_dir / "ldconfig"
    ldconfig.write_text(f"#!/usr/bin/env bash\nprintf '\\tliblcms2.so.2 (libc6,x86-64) => {system_library}\\n'\n")
    ldconfig.chmod(0o755)

    bundle_root = tmp_path / "AppDir" / "usr" / "lib" / "carvera-simulator"
    destination = bundle_root / "_internal"
    destination.mkdir(parents=True)
    result = subprocess.run(
        [
            "bash",
            "-c",
            'source "$1"; linux_bundle_shared_library "$2" "$3" "$4"',
            "bash",
            str(COMMON_SCRIPT),
            "liblcms2.so.2",
            str(bundle_root),
            str(destination),
        ],
        env={"PATH": f"{bin_dir}:/usr/bin:/bin"},
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    assert (destination / "liblcms2.so.2").read_bytes() == b"lcms2"


def test_appimage_launcher_selects_qt_and_enables_blocklisted_webgl(tmp_path: Path) -> None:
    app_dir = tmp_path / "Carvera-Simulator.AppDir"
    launcher = app_dir / "AppRun"
    executable = app_dir / "usr" / "lib" / "carvera-simulator" / "Carvera Simulator"
    executable.parent.mkdir(parents=True)
    shutil.copy2(APP_RUN, launcher)
    executable.write_text(
        "#!/usr/bin/env bash\n"
        "printf '%s\\n' \"$PYWEBVIEW_GUI\"\n"
        "printf '%s\\n' \"$QTWEBENGINE_CHROMIUM_FLAGS\"\n"
        "printf '%s\\n' \"$*\"\n"
    )
    executable.chmod(0o755)

    result = subprocess.run(
        [launcher, "--host", "127.0.0.1"],
        env={**os.environ, "QTWEBENGINE_CHROMIUM_FLAGS": "--disable-logging"},
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines() == [
        "qt",
        "--disable-logging --ignore-gpu-blocklist",
        "--host 127.0.0.1",
    ]
