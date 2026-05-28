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
from tempfile import TemporaryDirectory

from gui.scene.model_registry import BUNDLED_MODEL_MOUNT, CUSTOM_MODEL_MOUNT, MachineModelRegistry


class StaticFileApp:
    def __init__(self) -> None:
        self.mounts: list[tuple[str, str]] = []

    def add_static_files(self, mount: str, path: str) -> None:
        self.mounts.append((mount, path))


def test_model_registry_test() -> None:
    simulator_root = Path.cwd()
    static_app = StaticFileApp()
    registry = MachineModelRegistry(simulator_root=simulator_root, static_file_app=static_app, configured_model=None)

    assert static_app.mounts == [(BUNDLED_MODEL_MOUNT, str(simulator_root / "machine_models"))]
    c1_asset = registry.asset_for("c1")
    assert c1_asset is not None
    assert c1_asset.label == "carvera_c1.glb"
    assert c1_asset.kind == "gltf"
    assert c1_asset.machine_model == "c1"
    assert c1_asset.components is not None
    assert {"base", "x", "z", "y3", "y4", "a_chuck"} <= set(c1_asset.components)
    assert c1_asset.offset == (-141.5, 13.0, 86.0)
    assert c1_asset.rotation_degrees == (90.0, 0.0, 0.0)
    assert c1_asset.spindle_face_local is not None

    ca1_asset = registry.asset_for("ca1")
    assert ca1_asset is not None
    assert ca1_asset.label == "carvera_air_ca1.glb"
    assert ca1_asset.kind == "gltf"
    assert ca1_asset.machine_model == "ca1"
    assert ca1_asset.components is not None
    assert {"base", "x", "z", "y3", "y4", "a_chuck"} <= set(ca1_asset.components)
    assert ca1_asset.offset == (-82.5, -13.5, 33.0)
    assert ca1_asset.rotation_degrees == (90.0, 0.0, 0.0)
    assert ca1_asset.spindle_face_local == (58.597, 12.939, 49.0)

    remote_registry = MachineModelRegistry(
        simulator_root=simulator_root,
        static_file_app=StaticFileApp(),
        configured_model="https://example.test/model.glb",
    )
    remote_asset = remote_registry.asset_for("ca1")
    assert remote_asset is not None
    assert remote_asset.url == "https://example.test/model.glb"
    assert remote_asset.kind == "gltf"
    assert remote_asset.machine_model == "ca1"

    with TemporaryDirectory() as root:
        model_path = Path(root) / "Custom Machine.stl"
        model_path.write_bytes(b"solid model\nendsolid model\n")
        custom_app = StaticFileApp()
        custom_registry = MachineModelRegistry(
            simulator_root=simulator_root,
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
