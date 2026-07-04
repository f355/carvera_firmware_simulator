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

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_windows_builder_packages_native_gui_backend_and_runtime_dependencies() -> None:
    script = (ROOT / "scripts" / "build_windows_app.ps1").read_text()

    assert "carvera_sim_stream_stdio.exe" in script
    assert '"--windowed"' in script
    assert '"--onedir"' in script
    assert "machine_models" in script
    assert "default_sdcard" in script
    assert "objdump.exe" in script
    assert "DLL Name:" in script
    assert '"--add-binary"' in script
    assert "Get-UcrtRuntimeDlls" in script


def test_windows_packaging_has_an_isolated_dependency_group() -> None:
    project = (ROOT / "pyproject.toml").read_text()

    assert "package-windows = [" in project
    assert '"pyinstaller>=6.20.0"' in project
