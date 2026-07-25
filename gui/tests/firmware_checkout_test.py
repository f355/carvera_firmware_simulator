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
from pathlib import Path

import pytest

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="drives the POSIX firmware checkout script")

ROOT = Path(__file__).resolve().parents[2]


def seed_git_repository(source: Path) -> str:
    source.mkdir()
    (source / "src").mkdir()
    (source / "src" / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
    subprocess.run(["git", "init"], cwd=source, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=source, check=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=source, check=True)
    subprocess.run(["git", "add", "."], cwd=source, check=True)
    subprocess.run(
        ["git", "-c", "commit.gpgsign=false", "commit", "-m", "seed"],
        cwd=source,
        check=True,
        stdout=subprocess.DEVNULL,
    )
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()


def test_ensure_firmware_checkout_clones_requested_commit(tmp_path: Path) -> None:
    source = tmp_path / "source"
    commit = seed_git_repository(source)

    checkout = tmp_path / "checkout"
    env = {
        **os.environ,
        "CARVERA_SIM_FIRMWARE_REPO": str(source),
        "CARVERA_SIM_FIRMWARE_COMMIT": commit,
        "CARVERA_SIM_FIRMWARE_DIR": str(checkout),
    }
    result = subprocess.run(
        [str(ROOT / "scripts" / "ensure_firmware_checkout.sh")],
        check=True,
        env=env,
        stdout=subprocess.PIPE,
        text=True,
    )

    assert Path(result.stdout.strip()) == checkout
    checked_out = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=checkout, text=True).strip()
    assert checked_out == commit


def test_ensure_firmware_checkout_rejects_unpinned_user_checkout(tmp_path: Path) -> None:
    source = tmp_path / "source"
    pinned_commit = seed_git_repository(source)
    (source / "src" / "main.cpp").write_text("int main() { return 1; }\n", encoding="utf-8")
    subprocess.run(
        ["git", "-c", "commit.gpgsign=false", "commit", "-am", "move on"],
        cwd=source,
        check=True,
        stdout=subprocess.DEVNULL,
    )

    env = {**os.environ, "CARVERA_SIM_FIRMWARE_COMMIT": pinned_commit}
    rejected = subprocess.run(
        [str(ROOT / "scripts" / "ensure_firmware_checkout.sh"), "--firmware-root", str(source)],
        env=env,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert rejected.returncode != 0
    assert "does not match compatible commit" in rejected.stderr

    accepted = subprocess.run(
        [str(ROOT / "scripts" / "ensure_firmware_checkout.sh"), "--firmware-root", str(source), "--allow-unpinned"],
        check=True,
        env=env,
        stdout=subprocess.PIPE,
        text=True,
    )
    assert Path(accepted.stdout.strip()) == source
