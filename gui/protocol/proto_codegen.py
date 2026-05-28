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
from shutil import which
from pathlib import Path


SIMULATOR_ROOT = Path(__file__).resolve().parents[2]
PROTO_PATH = SIMULATOR_ROOT / "proto" / "carvera_sim.proto"
GENERATED_DIR = Path(__file__).resolve().parents[1] / "generated"
GENERATED_MODULE = GENERATED_DIR / "carvera_sim_pb2.py"
GENERATED_STUB = GENERATED_DIR / "carvera_sim_pb2.pyi"


def generated_proto_is_current() -> bool:
    return (
        GENERATED_MODULE.exists()
        and GENERATED_STUB.exists()
        and GENERATED_MODULE.stat().st_mtime >= PROTO_PATH.stat().st_mtime
        and GENERATED_STUB.stat().st_mtime >= PROTO_PATH.stat().st_mtime
    )


def ensure_proto_current() -> Path:
    """Check that Python protobuf bindings were generated explicitly."""
    if generated_proto_is_current():
        return GENERATED_DIR
    raise RuntimeError(
        "generated protobuf bindings are stale or missing; "
        "run `uv run python -m gui.protocol.proto_codegen` from the simulator repo"
    )


def generate_proto() -> Path:
    """Generate Python protobuf bindings from the checked-in schema."""
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    (GENERATED_DIR / "__init__.py").touch()

    if generated_proto_is_current():
        return GENERATED_DIR

    if which("protoc") is None:
        raise RuntimeError("protoc is required to generate Python protobuf bindings")

    subprocess.run(
        [
            "protoc",
            f"-I{PROTO_PATH.parent}",
            f"--python_out={GENERATED_DIR}",
            f"--pyi_out={GENERATED_DIR}",
            str(PROTO_PATH),
        ],
        check=True,
    )

    return GENERATED_DIR


def add_generated_to_path() -> Path:
    generated = ensure_proto_current()
    generated_text = str(generated)
    if generated_text not in sys.path:
        sys.path.insert(0, generated_text)
    return generated


def main() -> None:
    generate_proto()


if __name__ == "__main__":
    main()
