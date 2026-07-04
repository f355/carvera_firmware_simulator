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

from gui.core.platform_paths import open_in_file_manager, user_data_root


def test_user_data_root_uses_macos_application_support(tmp_path: Path) -> None:
    assert user_data_root(platform="darwin", home=tmp_path, env={}) == (
        tmp_path / "Library" / "Application Support" / "Carvera Simulator"
    )


def test_user_data_root_honors_xdg_data_home(tmp_path: Path) -> None:
    xdg_root = tmp_path / "xdg"
    assert user_data_root(platform="linux", home=tmp_path, env={"XDG_DATA_HOME": str(xdg_root)}) == (
        xdg_root / "carvera-simulator"
    )


def test_open_in_file_manager_creates_directory_and_uses_finder(tmp_path: Path) -> None:
    sd_root = tmp_path / "sdcard"
    calls: list[tuple[list[str], bool]] = []

    def run(command: list[str], *, check: bool) -> None:
        calls.append((command, check))

    open_in_file_manager(sd_root, platform="darwin", run=run)

    assert sd_root.is_dir()
    assert calls == [(["open", str(sd_root)], True)]
