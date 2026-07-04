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
from pathlib import Path

import pytest


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


@pytest.mark.parametrize(("machine", "appimage_architecture"), (("aarch64", "aarch64"), ("x86_64", "x86_64")))
def test_linux_packaging_maps_native_machine_names_to_appimage_architectures(
    machine: str, appimage_architecture: str
) -> None:
    assert run_common("linux_appimage_architecture", machine) == appimage_architecture


@pytest.mark.parametrize(("machine", "file_pattern"), (("aarch64", "ARM aarch64"), ("x86_64", "x86-64")))
def test_linux_packaging_uses_file_architecture_names(machine: str, file_pattern: str) -> None:
    assert run_common("linux_file_machine_pattern", machine) == file_pattern


def test_linux_appimage_has_desktop_metadata() -> None:
    desktop_file = (ROOT / "packaging" / "linux" / "carvera-simulator.desktop").read_text()
    app_run = (ROOT / "packaging" / "linux" / "AppRun").read_text()

    assert "Name=Carvera Simulator" in desktop_file
    assert "Exec=carvera-simulator" in desktop_file
    assert "usr/lib/carvera-simulator/Carvera Simulator" in app_run


def test_linux_builder_trusts_only_the_mounted_firmware_checkout() -> None:
    containerfile = (ROOT / "packaging" / "linux" / "Containerfile").read_text()

    assert "safe.directory /work/firmware/Carvera_Community_Firmware" in containerfile
    assert "safe.directory '*'" not in containerfile


def test_linux_builder_matches_the_ci_ubuntu_release() -> None:
    containerfile = (ROOT / "packaging" / "linux" / "Containerfile").read_text()
    container_wrapper = (ROOT / "scripts" / "build_linux_appimage_container.sh").read_text()

    assert "FROM ubuntu:24.04" in containerfile
    assert "appimage-builder:ubuntu-24.04" in container_wrapper


def test_linux_builder_bundles_qt_webengine_runtime_libraries() -> None:
    containerfile = (ROOT / "packaging" / "linux" / "Containerfile").read_text()
    build_script = (ROOT / "scripts" / "build_linux_appimage.sh").read_text()
    libraries = (
        "libasound.so.2",
        "liblcms2.so.2",
        "libminizip.so.1",
        "libopus.so.0",
        "libsnappy.so.1",
        "libwebp.so.7",
        "libwebpdemux.so.2",
        "libwebpmux.so.3",
        "libXfixes.so.3",
    )

    for package in (
        "libasound2t64",
        "liblcms2-2",
        "libminizip1t64",
        "libopus0",
        "libsnappy1v5",
        "libwebp7",
        "libwebpdemux2",
        "libwebpmux3",
        "libxfixes3",
    ):
        assert package in containerfile
    for library in libraries:
        assert library in build_script


def test_linux_builder_copies_a_system_library_omitted_by_pyinstaller(tmp_path: Path) -> None:
    system_library = tmp_path / "system" / "liblcms2.so.2.0.16"
    system_library.parent.mkdir()
    system_library.write_bytes(b"lcms2")

    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    ldconfig = bin_dir / "ldconfig"
    ldconfig.write_text(
        "#!/usr/bin/env bash\n"
        f"printf '\\tliblcms2.so.2 (libc6,x86-64) => {system_library}\\n'\n"
    )
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


def test_linux_smoke_test_terminates_the_native_gui_process_group() -> None:
    smoke_test = (ROOT / "scripts" / "smoke_test_linux_appimage.sh").read_text()

    assert "setsid xvfb-run" in smoke_test
    assert 'kill -TERM -- "-$APP_PID"' in smoke_test


def test_ci_packages_both_linux_architectures_for_the_development_release() -> None:
    workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text()

    assert "ubuntu-24.04-arm" in workflow
    assert "Carvera-Simulator-Linux-arm64.AppImage" in workflow
    assert "Carvera-Simulator-Linux-amd64.AppImage" in workflow
    assert "needs: [test, linux-appimage]" in workflow
