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

from gui.core.runtime_paths import resource_root


def test_resource_root_uses_pyinstaller_bundle_when_frozen(tmp_path: Path) -> None:
    assert resource_root(frozen=True, bundle_root=tmp_path) == tmp_path


def test_resource_root_finds_repository_from_module_path(tmp_path: Path) -> None:
    module_file = tmp_path / "repo" / "gui" / "core" / "runtime_paths.py"
    assert resource_root(module_file=module_file, frozen=False) == tmp_path / "repo"
