from __future__ import annotations

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / "machine_models"
CA1_SOURCE = MODEL_DIR / "carvera_air_ca1.glb"
C1_SOURCE = MODEL_DIR / "carvera_c1.glb"


def read_glb(path: Path) -> tuple[dict, bytes]:
    data = path.read_bytes()
    magic, version, total_length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67 or version != 2 or total_length != len(data):
        raise ValueError(f"{path} is not a GLB v2 file")

    offset = 12
    document = None
    binary = b""
    while offset + 8 <= len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == 0x4E4F534A:
            document = json.loads(chunk.rstrip(b" \t\r\n\x00"))
        elif chunk_type == 0x004E4942:
            binary = chunk
    if document is None:
        raise ValueError(f"{path} does not contain a JSON chunk")
    return document, binary


def write_glb(path: Path, document: dict, binary: bytes) -> None:
    json_chunk = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_chunk += b" " * ((4 - len(json_chunk) % 4) % 4)
    binary += b"\x00" * ((4 - len(binary) % 4) % 4)
    total_length = 12 + 8 + len(json_chunk) + 8 + len(binary)
    path.write_bytes(
        struct.pack("<III", 0x46546C67, 2, total_length)
        + struct.pack("<II", len(json_chunk), 0x4E4F534A)
        + json_chunk
        + struct.pack("<II", len(binary), 0x004E4942)
        + binary
    )


def copy_for_scene(source_document: dict, root_node: int, child_overrides: dict[int, list[int] | None]) -> dict:
    document = json.loads(json.dumps(source_document))
    document["scenes"] = [{"nodes": [root_node]}]
    document["scene"] = 0
    for node_index, children in child_overrides.items():
        if children is None:
            document["nodes"][node_index].pop("children", None)
        else:
            document["nodes"][node_index]["children"] = children
    return document


def apply_material(document: dict, color: tuple[float, float, float, float]) -> dict:
    document = json.loads(json.dumps(document))
    document["materials"] = [
        {
            "pbrMetallicRoughness": {
                "baseColorFactor": list(color),
                "metallicFactor": 0.0,
                "roughnessFactor": 0.58,
            },
        }
    ]
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            primitive["material"] = 0
    return document


def main() -> None:
    document, binary = read_glb(CA1_SOURCE)
    ca1_variants = {
        "carvera_air_ca1_base.glb": copy_for_scene(document, 1, {}),
        "carvera_air_ca1_x_axis.glb": copy_for_scene(document, 8, {8: [10]}),
        "carvera_air_ca1_z_axis.glb": copy_for_scene(document, 8, {8: [9]}),
        "carvera_air_ca1_y_axis_3.glb": copy_for_scene(document, 11, {11: [17]}),
        "carvera_air_ca1_y_axis_4.glb": copy_for_scene(document, 11, {11: [12, 17]}),
        "carvera_air_ca1_y_axis_4_static.glb": copy_for_scene(document, 11, {11: [12, 17], 12: [14, 15, 16]}),
        "carvera_air_ca1_a_chuck.glb": copy_for_scene(document, 11, {11: [12], 12: [13]}),
    }
    for name, variant in ca1_variants.items():
        write_glb(MODEL_DIR / name, apply_material(variant, (0.84, 0.87, 0.9, 1.0)), binary)

    document, binary = read_glb(C1_SOURCE)
    c1_variants = {
        "carvera_c1_base.glb": copy_for_scene(document, 0, {0: [18]}),
        # The source node name is "Z-Axis:1", but this whole carriage travels along firmware X.
        "carvera_c1_x_axis.glb": copy_for_scene(document, 1, {1: [3]}),
        # The source node name is "Spindle:1"; this is the Z-moving part.
        "carvera_c1_z_axis.glb": copy_for_scene(document, 1, {1: [2]}),
        "carvera_c1_y_axis_3.glb": copy_for_scene(document, 4, {4: [17]}),
        "carvera_c1_y_axis_4.glb": copy_for_scene(document, 4, {4: [5, 17]}),
        "carvera_c1_y_axis_4_static.glb": copy_for_scene(document, 4, {4: [5, 17], 5: [6, 7]}),
        "carvera_c1_a_chuck.glb": copy_for_scene(document, 4, {4: [5], 5: [8]}),
    }
    for name, variant in c1_variants.items():
        write_glb(MODEL_DIR / name, apply_material(variant, (0.84, 0.87, 0.9, 1.0)), binary)


if __name__ == "__main__":
    main()
