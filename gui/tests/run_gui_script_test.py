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
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_run_gui_builds_only_runtime_binary() -> None:
    script = (ROOT / "run_gui.sh").read_text(encoding="utf-8")
    if 'cmake --build "$BUILD_DIR" --target carvera_sim_stream_stdio --parallel "$(detect_build_jobs)"' not in script:
        raise SystemExit("run_gui.sh should build only the simulator binary needed by the GUI")
    if "CARVERA_SIM_BUILD_JOBS" not in script:
        raise SystemExit("run_gui.sh should let callers override build parallelism")
    if "-G Ninja" not in script:
        raise SystemExit("run_gui.sh should prefer Ninja for new build directories")


def test_run_gui_uses_managed_firmware_checkout() -> None:
    script = (ROOT / "run_gui.sh").read_text(encoding="utf-8")
    if "scripts/ensure_firmware_checkout.sh" not in script:
        raise SystemExit("run_gui.sh should use the pinned firmware checkout helper")
    if "CARVERA_SIM_FIRMWARE_DIR" not in script:
        raise SystemExit("run_gui.sh should let callers override the managed firmware checkout directory")
    if "uv run python -m gui.protocol.proto_codegen" not in script:
        raise SystemExit("run_gui.sh should explicitly refresh Python protobuf bindings before GUI import")


def test_cmake_uses_cxx20_and_cmake_protobuf_target() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    if "set(CMAKE_CXX_STANDARD 20)" not in cmake:
        raise SystemExit("CMake should build the simulator as C++20")
    if "pkg_check_modules(PROTOBUF_PC" in cmake:
        raise SystemExit("CMake should not require pkg-config for Protobuf")
    if "protobuf::libprotobuf" not in cmake:
        raise SystemExit("CMake should prefer the CMake Protobuf imported target")


def test_firmware_sources_are_auto_detected_by_cmake() -> None:
    sources = (ROOT / "cmake" / "FirmwareSources.cmake").read_text(encoding="utf-8")
    if "file(GLOB _matches CONFIGURE_DEPENDS" not in sources:
        raise SystemExit("firmware source inventory should auto-detect additions with CONFIGURE_DEPENDS globs")
    if "sim_glob_firmware_sources(FIRMWARE_MODULE_SOURCES" not in sources:
        raise SystemExit("firmware module source selection should live in one obvious CMake inventory")


def test_ci_runs_python_and_cpp_checks() -> None:
    workflow = ROOT / ".github" / "workflows" / "ci.yml"
    if not workflow.exists():
        raise SystemExit("public repo should have CI before push")
    ci = workflow.read_text(encoding="utf-8")
    for text in (
        "uv run ruff check gui",
        "uv run mypy gui",
        "uv run pytest gui/tests",
        "cmake --build build --target check --parallel ${{ env.CMAKE_BUILD_PARALLEL_LEVEL }}",
        "CMAKE_BUILD_PARALLEL_LEVEL",
        'WSL_WORKSPACE="/tmp/carvera_firmware_simulator_ci_${GITHUB_RUN_ID}_${GITHUB_RUN_ATTEMPT}"',
        'cd "$WSL_WORKSPACE"',
    ):
        if text not in ci:
            raise SystemExit(f"CI should run {text}")


def test_ensure_firmware_checkout_clones_requested_commit(tmp_path: Path) -> None:
    source = tmp_path / "source"
    source.mkdir()
    (source / "src").mkdir()
    (source / "src" / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
    subprocess.run(["git", "init"], cwd=source, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=source, check=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=source, check=True)
    subprocess.run(["git", "add", "."], cwd=source, check=True)
    subprocess.run(
        ["git", "-c", "commit.gpgsign=false", "commit", "-m", "seed"], cwd=source, check=True, stdout=subprocess.DEVNULL
    )
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()

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

    if Path(result.stdout.strip()) != checkout:
        raise SystemExit("helper should print the managed checkout path")
    checked_out = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=checkout, text=True).strip()
    if checked_out != commit:
        raise SystemExit("helper should checkout the configured firmware commit")


def test_ensure_firmware_checkout_rejects_unpinned_user_checkout(tmp_path: Path) -> None:
    source = tmp_path / "source"
    source.mkdir()
    (source / "src").mkdir()
    (source / "src" / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
    subprocess.run(["git", "init"], cwd=source, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=source, check=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=source, check=True)
    subprocess.run(["git", "add", "."], cwd=source, check=True)
    subprocess.run(
        ["git", "-c", "commit.gpgsign=false", "commit", "-m", "seed"], cwd=source, check=True, stdout=subprocess.DEVNULL
    )
    pinned_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
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
    if rejected.returncode == 0:
        raise SystemExit("helper should reject a user checkout at the wrong firmware commit")
    if "does not match compatible commit" not in rejected.stderr:
        raise SystemExit("helper should explain the compatible-commit mismatch")

    accepted = subprocess.run(
        [str(ROOT / "scripts" / "ensure_firmware_checkout.sh"), "--firmware-root", str(source), "--allow-unpinned"],
        check=True,
        env=env,
        stdout=subprocess.PIPE,
        text=True,
    )
    if Path(accepted.stdout.strip()) != source:
        raise SystemExit("--allow-unpinned should keep using the requested checkout")
