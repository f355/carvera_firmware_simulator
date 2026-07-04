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


def test_windows_builder_bundles_a_pinned_fixed_webview2_runtime() -> None:
    script = (ROOT / "scripts" / "build_windows_app.ps1").read_text()
    desktop = (ROOT / "gui" / "desktop.py").read_text()

    assert "Microsoft.WebView2.FixedVersionRuntime.150.0.4078.48.x64.cab" in script
    assert "9e347ba96d031e381d1041d1c20fd434d457875c422eeac3f40eee4a5e0ab5c0" in script
    assert '"--add-data"' in script
    assert '"--runtime-hook"' not in script
    assert "webview2" in script
    assert "configure_bundled_webview_runtime" in desktop
    assert "app.native.settings.update" in desktop
    assert 'app.native.start_args["gui"] = "edgechromium"' in desktop
    assert "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER" in (ROOT / "gui" / "core" / "webview_runtime.py").read_text()
    assert "prepare_fixed_runtime" in desktop
    assert '"*S-1-15-2-2:(OI)(CI)(RX)"' in script
    assert '"*S-1-15-2-1:(OI)(CI)(RX)"' in script


def test_windows_builder_replaces_the_final_app_without_nesting_stale_output() -> None:
    script = (ROOT / "scripts" / "build_windows_app.ps1").read_text()

    assert "Remove-Item -LiteralPath $OutputDir -Recurse -Force -ErrorAction SilentlyContinue" not in script
    assert 'Copy-Item -Path (Join-Path $PyInstallerApp "*") -Destination $FinalApp -Recurse' in script


def test_windows_packaging_has_an_isolated_dependency_group() -> None:
    project = (ROOT / "pyproject.toml").read_text()

    assert "package-windows = [" in project
    assert '"pyinstaller>=6.20.0"' in project


def test_ci_builds_and_publishes_the_native_windows_app() -> None:
    workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text()

    assert "windows-2022" in workflow
    assert "msys2/setup-msys2@" in workflow
    assert "msystem: UCRT64" in workflow
    assert "mingw-w64-ucrt-x86_64-gcc" in workflow
    assert "build_windows_app.ps1" in workflow
    assert "Carvera-Simulator-Windows-x64.zip" in workflow
    assert "needs: [test, linux-appimage, windows-app]" in workflow
    assert "Vampire/setup-wsl" not in workflow
