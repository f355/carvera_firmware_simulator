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
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote, urlparse

SUPPORTED_MODEL_EXTENSIONS = {
    ".glb": "gltf",
    ".gltf": "gltf",
    ".stl": "stl",
}


@dataclass(frozen=True)
class ModelBounds:
    minimum: tuple[float, float, float]
    maximum: tuple[float, float, float]


@dataclass(frozen=True)
class MachineModelAsset:
    url: str
    kind: str
    label: str
    machine_model: str | None = None
    bounds: ModelBounds | None = None
    components: dict[str, str] | None = None
    offset: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation_degrees: tuple[float, float, float] = (0.0, 0.0, 0.0)
    spindle_face_local: tuple[float, float, float] | None = None


def read_glb_bounds(path: Path) -> ModelBounds | None:
    data = path.read_bytes()
    if len(data) < 20:
        return None
    magic, version, total_length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67 or version != 2 or total_length > len(data):
        return None

    offset = 12
    while offset + 8 <= total_length:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type != 0x4E4F534A:
            continue
        document = json.loads(chunk.rstrip(b" \t\r\n\x00"))
        mins: list[list[float]] = []
        maxs: list[list[float]] = []
        for accessor in document.get("accessors", []):
            if accessor.get("type") == "VEC3" and "min" in accessor and "max" in accessor:
                mins.append(accessor["min"])
                maxs.append(accessor["max"])
        if not mins:
            return None
        minimum = (
            min(values[0] for values in mins),
            min(values[1] for values in mins),
            min(values[2] for values in mins),
        )
        maximum = (
            max(values[0] for values in maxs),
            max(values[1] for values in maxs),
            max(values[2] for values in maxs),
        )
        return ModelBounds(
            minimum=minimum,
            maximum=maximum,
        )
    return None


def machine_model_kind(location: str | Path) -> str:
    suffix = Path(str(urlparse(str(location)).path)).suffix.lower()
    try:
        return SUPPORTED_MODEL_EXTENSIONS[suffix]
    except KeyError as exc:
        supported = ", ".join(sorted(SUPPORTED_MODEL_EXTENSIONS))
        raise ValueError(f"unsupported machine model extension {suffix!r}; expected one of: {supported}") from exc


def is_remote_model(location: str | Path) -> bool:
    parsed = urlparse(str(location))
    return parsed.scheme in {"http", "https"}


def local_model_url(path: Path, mount: str) -> str:
    return f"{mount.rstrip('/')}/{quote(path.name)}"
