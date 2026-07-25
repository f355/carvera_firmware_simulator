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

import json
import struct
from pathlib import Path

import pytest

from gui.scene.machine_model_asset import is_remote_model, local_model_url, machine_model_kind, read_glb_bounds

PART_HIERARCHIES = {
    "carvera_air_ca1": {
        "base": ([1], {}),
        "x_axis": ([8], {8: [10]}),
        "z_axis": ([8], {8: [9]}),
        "y_axis_3": ([11], {11: [17]}),
        "y_axis_4": ([11], {11: [12, 17]}),
        "y_axis_4_static": ([11], {11: [12, 17], 12: [14, 15, 16]}),
        "a_chuck": ([11], {11: [12], 12: [13]}),
    },
    "carvera_c1": {
        "base": ([0], {0: [18]}),
        "x_axis": ([1], {1: [3]}),
        "z_axis": ([1], {1: [2]}),
        "y_axis_3": ([4], {4: [17]}),
        "y_axis_4": ([4], {4: [5, 17]}),
        "y_axis_4_static": ([4], {4: [5, 17], 5: [6, 7]}),
        "a_chuck": ([4], {4: [5], 5: [8]}),
    },
}


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


@pytest.mark.parametrize(
    ("path", "expected"),
    [
        ("shell.glb", "gltf"),
        ("shell.gltf", "gltf"),
        ("shell.stl", "stl"),
        ("https://example.test/model.glb?cache=1", "gltf"),
    ],
)
def test_machine_model_kind_recognizes_browser_formats(path: str, expected: str) -> None:
    assert machine_model_kind(path) == expected


def test_machine_model_kind_rejects_step_files() -> None:
    with pytest.raises(ValueError, match=r"\.glb"):
        machine_model_kind("shell.step")


def test_model_urls_distinguish_remote_and_local_assets() -> None:
    assert is_remote_model("https://example.test/model.glb")
    assert not is_remote_model(Path("machine_models/carvera_air_ca1.glb"))
    assert local_model_url(Path("Carvera Air.glb"), "/machine_model_asset") == "/machine_model_asset/Carvera%20Air.glb"


@pytest.mark.parametrize("model", ["carvera_air_ca1", "carvera_c1"])
def test_bundled_machine_model_has_nonempty_bounds(model: str) -> None:
    bounds = read_glb_bounds(Path(f"machine_models/{model}.glb"))
    assert bounds is not None
    assert bounds.minimum[0] < bounds.maximum[0]


@pytest.mark.parametrize(
    ("model", "part"),
    [(model, part) for model, parts in PART_HIERARCHIES.items() for part in parts],
)
def test_bundled_split_model_preserves_part_hierarchy(model: str, part: str) -> None:
    path = Path(f"machine_models/{model}_{part}.glb")
    assert path.exists()

    expected_roots, expected_children = PART_HIERARCHIES[model][part]
    roots, children = glb_scene(path)
    assert roots == expected_roots
    for node, expected in expected_children.items():
        assert children[node] == expected
