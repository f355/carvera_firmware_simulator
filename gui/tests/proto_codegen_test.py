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
from pathlib import Path

import pytest

from gui.protocol import proto_codegen


def test_generated_proto_path_rejects_stale_bindings_without_running_protoc(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    proto = tmp_path / "carvera_sim.proto"
    generated = tmp_path / "generated"
    module = generated / "carvera_sim_pb2.py"
    stub = generated / "carvera_sim_pb2.pyi"
    generated.mkdir()
    proto.write_text('syntax = "proto3";\n', encoding="utf-8")
    module.write_text("# old generated module\n", encoding="utf-8")
    stub.write_text("# old generated stub\n", encoding="utf-8")

    old = proto.stat().st_mtime - 10
    os.utime(module, (old, old))
    os.utime(stub, (old, old))

    protoc_calls: list[object] = []
    monkeypatch.setattr(proto_codegen, "PROTO_PATH", proto)
    monkeypatch.setattr(proto_codegen, "GENERATED_DIR", generated)
    monkeypatch.setattr(proto_codegen, "GENERATED_MODULE", module)
    monkeypatch.setattr(proto_codegen, "GENERATED_STUB", stub)
    monkeypatch.setattr(proto_codegen, "which", lambda _name: "/usr/bin/protoc")
    monkeypatch.setattr(proto_codegen.subprocess, "run", lambda *args, **kwargs: protoc_calls.append((args, kwargs)))

    with pytest.raises(RuntimeError, match="generated protobuf bindings are stale"):
        proto_codegen.add_generated_to_path()

    assert protoc_calls == []
