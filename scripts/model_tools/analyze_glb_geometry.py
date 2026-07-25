from __future__ import annotations

import argparse
import json
import math
import struct
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

COMPONENT_FORMATS = {
    5121: "B",
    5122: "h",
    5123: "H",
    5125: "I",
    5126: "f",
}
TYPE_COUNTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
}
Matrix = tuple[
    tuple[float, float, float, float],
    tuple[float, float, float, float],
    tuple[float, float, float, float],
    tuple[float, float, float, float],
]


@dataclass(frozen=True)
class Transform:
    rotation_x_degrees: float
    offset: tuple[float, float, float]
    scale: float

    def point(self, point: tuple[float, float, float]) -> tuple[float, float, float]:
        x, y, z = (coordinate * self.scale for coordinate in point)
        radians = math.radians(self.rotation_x_degrees)
        rotated_y = y * math.cos(radians) - z * math.sin(radians)
        rotated_z = y * math.sin(radians) + z * math.cos(radians)
        return (x + self.offset[0], rotated_y + self.offset[1], rotated_z + self.offset[2])


@dataclass
class SurfaceBucket:
    area: float
    minimum: list[float]
    maximum: list[float]
    nodes: Counter[int]


def vector_sub(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (left[0] - right[0], left[1] - right[1], left[2] - right[2])


def vector_cross(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    )


def vector_length(vector: tuple[float, float, float]) -> float:
    return math.sqrt(sum(component * component for component in vector))


def triangle_area(triangle: list[tuple[float, float, float]]) -> float:
    return 0.5 * vector_length(vector_cross(vector_sub(triangle[1], triangle[0]), vector_sub(triangle[2], triangle[0])))


def triangle_normal(triangle: list[tuple[float, float, float]]) -> tuple[float, float, float]:
    normal = vector_cross(vector_sub(triangle[1], triangle[0]), vector_sub(triangle[2], triangle[0]))
    length = vector_length(normal)
    if length == 0:
        return (0.0, 0.0, 0.0)
    return (normal[0] / length, normal[1] / length, normal[2] / length)


def identity_matrix() -> Matrix:
    return (
        (1.0, 0.0, 0.0, 0.0),
        (0.0, 1.0, 0.0, 0.0),
        (0.0, 0.0, 1.0, 0.0),
        (0.0, 0.0, 0.0, 1.0),
    )


def multiply_matrix(left: Matrix, right: Matrix) -> Matrix:
    return tuple(
        tuple(
            sum(left[row][column_index] * right[column_index][column] for column_index in range(4))
            for column in range(4)
        )
        for row in range(4)
    )  # type: ignore[return-value]


def translation_matrix(translation: list[float]) -> Matrix:
    matrix = [list(row) for row in identity_matrix()]
    matrix[0][3], matrix[1][3], matrix[2][3] = translation
    return tuple(tuple(row) for row in matrix)  # type: ignore[return-value]


def scale_matrix(scale: list[float]) -> Matrix:
    return (
        (scale[0], 0.0, 0.0, 0.0),
        (0.0, scale[1], 0.0, 0.0),
        (0.0, 0.0, scale[2], 0.0),
        (0.0, 0.0, 0.0, 1.0),
    )


def rotation_matrix(quaternion: list[float]) -> Matrix:
    x, y, z, w = quaternion
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return (
        (1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy), 0.0),
        (2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx), 0.0),
        (2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy), 0.0),
        (0.0, 0.0, 0.0, 1.0),
    )


def gltf_matrix(values: list[float]) -> Matrix:
    # glTF stores node matrices in column-major order.
    return tuple(tuple(float(values[column * 4 + row]) for column in range(4)) for row in range(4))  # type: ignore[return-value]


def transform_point(matrix: Matrix, point: tuple[float, ...]) -> tuple[float, float, float]:
    x, y, z = point[:3]
    return (
        matrix[0][0] * x + matrix[0][1] * y + matrix[0][2] * z + matrix[0][3],
        matrix[1][0] * x + matrix[1][1] * y + matrix[1][2] * z + matrix[1][3],
        matrix[2][0] * x + matrix[2][1] * y + matrix[2][2] * z + matrix[2][3],
    )


class Glb:
    def __init__(self, path: Path) -> None:
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

        self.path = path
        self.document: dict[str, Any] = document
        self.binary = binary
        self.parents: dict[int, int] = {}
        for index, node in enumerate(self.document.get("nodes", [])):
            for child in node.get("children", []):
                self.parents[child] = index

    def accessor(self, index: int) -> list[tuple[float, ...]]:
        accessor = self.document["accessors"][index]
        view = self.document["bufferViews"][accessor["bufferView"]]
        component_format = COMPONENT_FORMATS[accessor["componentType"]]
        component_count = TYPE_COUNTS[accessor["type"]]
        item_format = "<" + component_format * component_count
        item_size = struct.calcsize(item_format)
        stride = view.get("byteStride", item_size)
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        return [
            struct.unpack_from(item_format, self.binary, start + item * stride) for item in range(accessor["count"])
        ]

    def node_local_matrix(self, index: int) -> Matrix:
        node = self.document["nodes"][index]
        if "matrix" in node:
            return gltf_matrix(node["matrix"])

        translation = translation_matrix(node.get("translation", [0.0, 0.0, 0.0]))
        rotation = rotation_matrix(node.get("rotation", [0.0, 0.0, 0.0, 1.0]))
        scale = scale_matrix(node.get("scale", [1.0, 1.0, 1.0]))
        return multiply_matrix(multiply_matrix(translation, rotation), scale)

    def node_world_matrix(self, index: int) -> Matrix:
        stack = []
        current: int | None = index
        while current is not None:
            stack.append(current)
            current = self.parents.get(current)

        matrix = identity_matrix()
        for node_index in reversed(stack):
            matrix = multiply_matrix(matrix, self.node_local_matrix(node_index))
        return matrix

    def node_vertices(self, node_index: int, primitive: dict[str, Any]) -> list[tuple[float, float, float]]:
        matrix = self.node_world_matrix(node_index)
        return [transform_point(matrix, vertex) for vertex in self.accessor(primitive["attributes"]["POSITION"])]

    def primitive_indices(self, primitive: dict[str, Any], vertex_count: int) -> list[int]:
        if "indices" not in primitive:
            return list(range(vertex_count))
        return [int(index[0]) for index in self.accessor(primitive["indices"])]

    def mesh_nodes(self) -> list[tuple[int, dict[str, Any]]]:
        return [(index, node) for index, node in enumerate(self.document.get("nodes", [])) if "mesh" in node]

    def resolve_node_filter(self, selectors: list[str]) -> set[int]:
        if not selectors:
            return {index for index, _node in self.mesh_nodes()}

        selected: set[int] = set()
        nodes = self.document.get("nodes", [])
        for selector in selectors:
            if selector.isdigit():
                index = int(selector)
                if index >= len(nodes):
                    raise ValueError(f"node index {index} is outside the model")
                selected.add(index)
                continue

            matches = [
                index
                for index, node in enumerate(nodes)
                if selector.lower() in (node.get("name") or "").lower() and "mesh" in node
            ]
            if not matches:
                raise ValueError(f"node selector {selector!r} did not match any mesh node")
            selected.update(matches)
        return selected


def extend_bounds(bucket: SurfaceBucket, point: tuple[float, float, float]) -> None:
    for axis in range(3):
        bucket.minimum[axis] = min(bucket.minimum[axis], point[axis])
        bucket.maximum[axis] = max(bucket.maximum[axis], point[axis])


def horizontal_surfaces(glb: Glb, transform: Transform, selected_nodes: set[int]) -> list[tuple[float, SurfaceBucket]]:
    buckets: dict[float, SurfaceBucket] = defaultdict(
        lambda: SurfaceBucket(area=0.0, minimum=[math.inf] * 3, maximum=[-math.inf] * 3, nodes=Counter())
    )
    for node_index, node in glb.mesh_nodes():
        if node_index not in selected_nodes:
            continue
        mesh = glb.document["meshes"][node["mesh"]]
        for primitive in mesh["primitives"]:
            vertices = [transform.point(vertex) for vertex in glb.node_vertices(node_index, primitive)]
            indices = glb.primitive_indices(primitive, len(vertices))
            for triangle_start in range(0, len(indices) - 2, 3):
                triangle = [vertices[indices[triangle_start + offset]] for offset in range(3)]
                area = triangle_area(triangle)
                if area < 1.0:
                    continue
                normal = triangle_normal(triangle)
                if abs(normal[2]) < 0.92:
                    continue
                z_key = round((sum(point[2] for point in triangle) / 3) / 5) * 5.0
                bucket = buckets[z_key]
                bucket.area += area
                bucket.nodes[node_index] += area
                for point in triangle:
                    extend_bounds(bucket, point)
    return sorted(buckets.items(), key=lambda item: item[1].area, reverse=True)


def node_bounds(
    glb: Glb, transform: Transform, selected_nodes: set[int]
) -> list[tuple[int, str, list[float], list[float]]]:
    output = []
    for node_index, node in glb.mesh_nodes():
        if node_index not in selected_nodes:
            continue
        points = []
        mesh = glb.document["meshes"][node["mesh"]]
        for primitive in mesh["primitives"]:
            points.extend(transform.point(vertex) for vertex in glb.node_vertices(node_index, primitive))
        minimum = [min(point[axis] for point in points) for axis in range(3)]
        maximum = [max(point[axis] for point in points) for axis in range(3)]
        output.append((node_index, node.get("name") or "", minimum, maximum))
    return output


def node_horizontal_surfaces(
    glb: Glb, transform: Transform, selected_nodes: set[int], surface_count: int
) -> list[tuple[int, str, list[tuple[float, SurfaceBucket]]]]:
    output = []
    for node_index, node in glb.mesh_nodes():
        if node_index not in selected_nodes:
            continue
        surfaces = horizontal_surfaces(glb, transform, {node_index})[:surface_count]
        if surfaces:
            output.append((node_index, node.get("name") or "", surfaces))
    return output


def round_values(values: list[float]) -> list[float]:
    return [round(value, 3) for value in values]


def print_surface(prefix: str, z_key: float, bucket: SurfaceBucket) -> None:
    size = [bucket.maximum[axis] - bucket.minimum[axis] for axis in range(3)]
    node_summary = ", ".join(f"{node}:{area:.0f}" for node, area in bucket.nodes.most_common(3))
    print(
        f"{prefix}z~{z_key:>7.1f} area={bucket.area:>10.1f} "
        f"min={round_values(bucket.minimum)} max={round_values(bucket.maximum)} "
        f"size={round_values(size)} nodes=[{node_summary}]"
    )


def print_report(glb: Glb, transform: Transform, surface_count: int, selected_nodes: set[int]) -> None:
    print(f"model: {glb.path}")
    print(
        f"transform: scale={transform.scale:g}, rotation_x={transform.rotation_x_degrees:g}, offset={transform.offset}"
    )
    print(f"selected mesh nodes: {', '.join(str(index) for index in sorted(selected_nodes))}")
    print("\nmesh node bounds:")
    for node_index, name, minimum, maximum in node_bounds(glb, transform, selected_nodes):
        size = [maximum[axis] - minimum[axis] for axis in range(3)]
        print(
            f"  {node_index:>2} {name or '<unnamed>':<34} min={round_values(minimum)} max={round_values(maximum)} size={round_values(size)}"
        )

    print("\nlargest horizontal surface buckets:")
    for z_key, bucket in horizontal_surfaces(glb, transform, selected_nodes)[:surface_count]:
        print_surface("  ", z_key, bucket)

    print("\nper-node horizontal surface buckets:")
    for node_index, name, surfaces in node_horizontal_surfaces(glb, transform, selected_nodes, min(surface_count, 4)):
        print(f"  {node_index:>2} {name or '<unnamed>'}")
        for z_key, bucket in surfaces:
            print_surface("    ", z_key, bucket)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect GLB geometry after GUI-style transforms.")
    parser.add_argument("model", type=Path)
    parser.add_argument("--scale", type=float, default=1000.0)
    parser.add_argument("--rotation-x", type=float, default=0.0)
    parser.add_argument("--offset", default="0,0,0", help="comma-separated scene offset")
    parser.add_argument("--nodes", default="", help="comma-separated node indices or case-insensitive name fragments")
    parser.add_argument("--surfaces", type=int, default=12)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    offset_parts = tuple(float(part.strip()) for part in args.offset.split(","))
    if len(offset_parts) != 3:
        raise SystemExit("--offset must contain three comma-separated numbers")
    glb = Glb(args.model)
    selected_nodes = glb.resolve_node_filter(
        [selector.strip() for selector in args.nodes.split(",") if selector.strip()]
    )
    print_report(
        glb,
        Transform(rotation_x_degrees=args.rotation_x, offset=offset_parts, scale=args.scale),
        args.surfaces,
        selected_nodes,
    )


if __name__ == "__main__":
    main()
