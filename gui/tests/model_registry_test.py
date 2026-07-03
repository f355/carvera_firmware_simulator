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

import pytest

from gui.scene.model_registry import BUNDLED_MODEL_MOUNT, CUSTOM_MODEL_MOUNT, MachineModelRegistry


class StaticFileApp:
    def __init__(self) -> None:
        self.mounts: list[tuple[str, str]] = []

    def add_static_files(self, mount: str, path: str) -> None:
        self.mounts.append((mount, path))


def make_registry(configured_model: str | None = None) -> tuple[MachineModelRegistry, StaticFileApp]:
    simulator_root = Path.cwd()
    static_app = StaticFileApp()
    registry = MachineModelRegistry(
        simulator_root=simulator_root,
        static_file_app=static_app,
        configured_model=configured_model,
    )
    return registry, static_app


@pytest.mark.parametrize(
    ("model", "label", "offset", "spindle_face"),
    [
        ("c1", "carvera_c1.glb", (-141.5, 13.0, 86.0), None),
        ("ca1", "carvera_air_ca1.glb", (-82.5, -13.5, 33.0), (58.597, 12.939, 49.0)),
    ],
)
def test_registry_exposes_bundled_split_models(
    model: str,
    label: str,
    offset: tuple[float, float, float],
    spindle_face: tuple[float, float, float] | None,
) -> None:
    registry, static_app = make_registry()
    simulator_root = Path.cwd()

    assert static_app.mounts == [(BUNDLED_MODEL_MOUNT, str(simulator_root / "machine_models"))]
    asset = registry.asset_for(model)
    assert asset is not None
    assert asset.label == label
    assert asset.kind == "gltf"
    assert asset.machine_model == model
    assert asset.components is not None
    assert {"base", "x", "z", "y3", "y4", "a_chuck"} <= set(asset.components)
    assert asset.offset == offset
    assert asset.rotation_degrees == (90.0, 0.0, 0.0)
    if spindle_face is None:
        assert asset.spindle_face_local is not None
    else:
        assert asset.spindle_face_local == spindle_face


def test_registry_accepts_remote_models() -> None:
    remote_registry, _ = make_registry("https://example.test/model.glb")
    remote_asset = remote_registry.asset_for("ca1")
    assert remote_asset is not None
    assert remote_asset.url == "https://example.test/model.glb"
    assert remote_asset.kind == "gltf"
    assert remote_asset.machine_model == "ca1"


def test_registry_mounts_local_custom_models(tmp_path: Path) -> None:
    model_path = tmp_path / "Custom Machine.stl"
    model_path.write_bytes(b"solid model\nendsolid model\n")
    custom_app = StaticFileApp()
    custom_registry = MachineModelRegistry(
        simulator_root=Path.cwd(),
        static_file_app=custom_app,
        configured_model=str(model_path),
        offset_override=(1.0, 2.0, 3.0),
    )

    custom_asset = custom_registry.asset_for("c1")
    assert custom_asset is not None
    assert custom_asset.url == f"{CUSTOM_MODEL_MOUNT}/Custom%20Machine.stl"
    assert custom_asset.machine_model == "c1"
    assert custom_asset.offset == (1.0, 2.0, 3.0)
    assert custom_app.mounts[-1] == (CUSTOM_MODEL_MOUNT, str(model_path.parent.resolve()))
