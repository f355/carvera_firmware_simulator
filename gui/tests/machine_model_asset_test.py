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
import json
import struct

from gui.scene.machine_model_asset import is_remote_model, local_model_url, machine_model_kind, read_glb_bounds


def glb_scene(path: Path) -> tuple[list[int], dict[int, list[int]]]:
    data = path.read_bytes()
    offset = 12
    while offset + 8 <= len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == 0x4E4F534A:
            document = json.loads(chunk.rstrip(b" \t\r\n\x00"))
            roots = list(document["scenes"][document.get("scene", 0)]["nodes"])
            children = {
                index: list(node.get("children", []))
                for index, node in enumerate(document.get("nodes", []))
                if node.get("children")
            }
            return roots, children
    raise AssertionError(f"GLB JSON chunk not found: {path}")


def test_machine_model_asset_test() -> None:
    assert machine_model_kind("shell.glb") == "gltf"
    assert machine_model_kind("shell.gltf") == "gltf"
    assert machine_model_kind("shell.stl") == "stl"
    assert machine_model_kind("https://example.test/model.glb?cache=1") == "gltf"
    assert is_remote_model("https://example.test/model.glb")
    assert not is_remote_model(Path("machine_models/carvera_air_ca1.glb"))
    assert local_model_url(Path("Carvera Air.glb"), "/machine_model_asset") == "/machine_model_asset/Carvera%20Air.glb"
    bounds = read_glb_bounds(Path("machine_models/carvera_air_ca1.glb"))
    assert bounds is not None
    assert bounds.minimum[0] < bounds.maximum[0]
    bounds = read_glb_bounds(Path("machine_models/carvera_c1.glb"))
    assert bounds is not None
    assert bounds.minimum[0] < bounds.maximum[0]
    for name in ("base", "x_axis", "z_axis", "y_axis_3", "y_axis_4", "y_axis_4_static", "a_chuck"):
        assert Path(f"machine_models/carvera_air_ca1_{name}.glb").exists()
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_base.glb"))
    assert roots == [1]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_x_axis.glb"))
    assert roots == [8]
    assert children[8] == [10]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_z_axis.glb"))
    assert roots == [8]
    assert children[8] == [9]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_y_axis_3.glb"))
    assert roots == [11]
    assert children[11] == [17]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_y_axis_4.glb"))
    assert roots == [11]
    assert children[11] == [12, 17]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_y_axis_4_static.glb"))
    assert roots == [11]
    assert children[11] == [12, 17]
    assert children[12] == [14, 15, 16]
    roots, children = glb_scene(Path("machine_models/carvera_air_ca1_a_chuck.glb"))
    assert roots == [11]
    assert children[11] == [12]
    assert children[12] == [13]
    for name in ("base", "x_axis", "z_axis", "y_axis_3", "y_axis_4", "y_axis_4_static", "a_chuck"):
        assert Path(f"machine_models/carvera_c1_{name}.glb").exists()
    roots, children = glb_scene(Path("machine_models/carvera_c1_base.glb"))
    assert roots == [0]
    assert children[0] == [18]
    roots, children = glb_scene(Path("machine_models/carvera_c1_x_axis.glb"))
    assert roots == [1]
    assert children[1] == [3]
    roots, children = glb_scene(Path("machine_models/carvera_c1_z_axis.glb"))
    assert roots == [1]
    assert children[1] == [2]
    roots, children = glb_scene(Path("machine_models/carvera_c1_y_axis_3.glb"))
    assert roots == [4]
    assert children[4] == [17]
    roots, children = glb_scene(Path("machine_models/carvera_c1_y_axis_4.glb"))
    assert roots == [4]
    assert children[4] == [5, 17]
    roots, children = glb_scene(Path("machine_models/carvera_c1_y_axis_4_static.glb"))
    assert roots == [4]
    assert children[4] == [5, 17]
    assert children[5] == [6, 7]
    roots, children = glb_scene(Path("machine_models/carvera_c1_a_chuck.glb"))
    assert roots == [4]
    assert children[4] == [5]
    assert children[5] == [8]
    try:
        machine_model_kind("shell.step")
    except ValueError as exc:
        assert ".glb" in str(exc)
    else:
        raise AssertionError("STEP should not be treated as a directly loadable browser mesh")
