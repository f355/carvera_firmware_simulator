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
from typing import Any

from .machine_model_asset import (
    MachineModelAsset,
    is_remote_model,
    local_model_url,
    machine_model_kind,
    read_glb_bounds,
)
from .machine_visual_spec import VISUAL_SPECS

BUNDLED_MODEL_MOUNT = "/machine_models"
CUSTOM_MODEL_MOUNT = "/machine_model_asset"


class MachineModelRegistry:
    def __init__(
        self,
        *,
        simulator_root: Path,
        static_file_app: Any,
        configured_model: str | None,
        offset_override: tuple[float, float, float] | None = None,
        bundled_mount: str = BUNDLED_MODEL_MOUNT,
        custom_mount: str = CUSTOM_MODEL_MOUNT,
    ) -> None:
        self.bundled_model_dir = simulator_root / "machine_models"
        self.static_file_app = static_file_app
        self.configured_model = configured_model
        self.offset_override = offset_override
        self.bundled_mount = bundled_mount
        self.custom_mount = custom_mount
        self.registered_model_dirs: set[Path] = set()
        if self.bundled_model_dir.exists():
            self.static_file_app.add_static_files(self.bundled_mount, str(self.bundled_model_dir))
            self.registered_model_dirs.add(self.bundled_model_dir.resolve())

    def asset_for(self, machine_model: str) -> MachineModelAsset | None:
        if self.configured_model is None:
            if machine_model == "c1":
                return self.c1_bundled_asset()
            if machine_model == "ca1":
                return self.ca1_bundled_asset()
            return None

        kind = machine_model_kind(self.configured_model)
        if is_remote_model(self.configured_model):
            return MachineModelAsset(
                url=str(self.configured_model),
                kind=kind,
                label=str(self.configured_model),
                machine_model=machine_model,
            )

        path = Path(self.configured_model).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"machine model asset does not exist: {path}")
        if path.parent not in self.registered_model_dirs:
            self.static_file_app.add_static_files(self.custom_mount, str(path.parent))
            self.registered_model_dirs.add(path.parent)
        return MachineModelAsset(
            url=local_model_url(path, self.custom_mount),
            kind=kind,
            label=path.name,
            machine_model=machine_model,
            bounds=read_glb_bounds(path),
            offset=self.offset_override or (0.0, 0.0, 0.0),
        )

    def c1_bundled_asset(self) -> MachineModelAsset | None:
        bundled = self.bundled_model_dir / "carvera_c1.glb"
        if not bundled.exists():
            return None
        return self._bundled_asset(
            bundled=bundled,
            machine_model="c1",
            filenames={
                "base": "carvera_c1_base.glb",
                "x": "carvera_c1_x_axis.glb",
                "z": "carvera_c1_z_axis.glb",
                "y3": "carvera_c1_y_axis_3.glb",
                "y4": "carvera_c1_y_axis_4_static.glb",
                "a_chuck": "carvera_c1_a_chuck.glb",
            },
        )

    def ca1_bundled_asset(self) -> MachineModelAsset | None:
        bundled = self.bundled_model_dir / "carvera_air_ca1.glb"
        if not bundled.exists():
            return None
        return self._bundled_asset(
            bundled=bundled,
            machine_model="ca1",
            filenames={
                "base": "carvera_air_ca1_base.glb",
                "x": "carvera_air_ca1_x_axis.glb",
                "z": "carvera_air_ca1_z_axis.glb",
                "y3": "carvera_air_ca1_y_axis_3.glb",
                "y4": "carvera_air_ca1_y_axis_4_static.glb",
                "a_chuck": "carvera_air_ca1_a_chuck.glb",
            },
        )

    def _bundled_asset(
        self,
        *,
        bundled: Path,
        machine_model: str,
        filenames: dict[str, str],
    ) -> MachineModelAsset:
        component_paths = {
            name: self.bundled_model_dir / filename
            for name, filename in filenames.items()
            if (self.bundled_model_dir / filename).exists()
        }
        components = {name: local_model_url(path, self.bundled_mount) for name, path in component_paths.items()}
        spec = VISUAL_SPECS[machine_model]
        return MachineModelAsset(
            url=local_model_url(bundled, self.bundled_mount),
            kind=machine_model_kind(bundled),
            label=bundled.name,
            machine_model=machine_model,
            bounds=read_glb_bounds(bundled),
            components=components or None,
            offset=spec.model_offset,
            rotation_degrees=spec.model_rotation_degrees,
            spindle_face_local=spec.spindle_face_local,
        )
