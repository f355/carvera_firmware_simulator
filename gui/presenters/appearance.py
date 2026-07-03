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

from typing import Any

from gui.app_view import AppView
from gui.presenters.base import SessionPresenter
from gui.scene.lighting import (
    DEFAULT_MODEL_COLOR,
    ModelMaterialSettings,
    SceneLightingSettings,
    apply_scene_lighting,
)
from gui.views.ui_helpers import event_bool


class AppearancePresenter(SessionPresenter):
    def scene_appearance_changed(self, view: AppView, _event: Any = None) -> None:
        if view.machine_scene_view is None:
            return
        apply_scene_lighting(view.machine_scene_view.scene, self.current_lighting_settings(view))
        view.machine_scene_view.apply_model_material(self.current_material_settings(view))

    def current_lighting_settings(self, view: AppView) -> SceneLightingSettings:
        controls = view.environment_tab_view.lighting_controls if view.environment_tab_view is not None else {}
        defaults = SceneLightingSettings()
        return SceneLightingSettings(
            ambient=self.control_float(controls, "ambient", defaults.ambient),
            key=self.control_float(controls, "key", defaults.key),
            key_x=self.control_float(controls, "key_x", defaults.key_x),
            key_y=self.control_float(controls, "key_y", defaults.key_y),
            key_z=self.control_float(controls, "key_z", defaults.key_z),
            fill=self.control_float(controls, "fill", defaults.fill),
            fill_x=self.control_float(controls, "fill_x", defaults.fill_x),
            fill_y=self.control_float(controls, "fill_y", defaults.fill_y),
            fill_z=self.control_float(controls, "fill_z", defaults.fill_z),
            exposure=self.control_float(controls, "exposure", defaults.exposure),
            shadows=event_bool(controls["shadows"]) if "shadows" in controls else defaults.shadows,
            shadow_radius=self.control_float(controls, "shadow_radius", defaults.shadow_radius),
            shadow_bias=self.control_float(controls, "shadow_bias", defaults.shadow_bias),
            shadow_map_size=self.control_float(controls, "shadow_map_size", defaults.shadow_map_size),
        )

    def current_material_settings(self, view: AppView) -> ModelMaterialSettings:
        controls = view.environment_tab_view.material_controls if view.environment_tab_view is not None else {}
        defaults = ModelMaterialSettings()
        color = str(controls["color"].value) if "color" in controls and controls["color"].value else DEFAULT_MODEL_COLOR
        return ModelMaterialSettings(
            color=color,
            opacity=self.control_float(controls, "opacity", defaults.opacity),
            roughness=self.control_float(controls, "roughness", defaults.roughness),
            metalness=self.control_float(controls, "metalness", defaults.metalness),
        )

    @staticmethod
    def control_float(controls: dict[str, Any], name: str, default: float) -> float:
        control = controls.get(name)
        if control is None:
            return default
        try:
            return float(control.value)
        except (TypeError, ValueError):
            return default
