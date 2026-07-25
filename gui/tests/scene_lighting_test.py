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

from gui.scene.lighting import (
    ModelMaterialSettings,
    SceneLightingSettings,
    scene_lighting_patch_javascript,
    scene_material_patch_javascript,
)


def test_scene_lighting_defaults_are_tuned_for_machine_models() -> None:
    defaults = SceneLightingSettings()
    expected_defaults = {
        "ambient": 0.2,
        "key": 0.5,
        "fill": 0.3,
        "shadow_bias": -0.003,
        "shadow_map_size": 1536.0,
        "key_x": -50.0,
        "key_y": -250.0,
        "key_z": 250.0,
        "fill_x": 40.0,
        "fill_y": 170.0,
        "fill_z": 160.0,
    }
    for name, expected_value in expected_defaults.items():
        assert getattr(defaults, name) == expected_value

    material_defaults = ModelMaterialSettings()
    assert material_defaults.roughness == 0.45
    assert material_defaults.metalness == 0.1


def test_lighting_patch_configures_managed_three_js_lights() -> None:
    script = scene_lighting_patch_javascript(
        42,
        SceneLightingSettings(
            ambient=0.2,
            key=0.9,
            key_x=-100.0,
            key_y=-200.0,
            key_z=300.0,
            fill=0.1,
            fill_x=100.0,
            fill_y=200.0,
            fill_z=150.0,
            exposure=1.1,
            shadows=True,
            shadow_radius=5.0,
            shadow_bias=-0.001,
            shadow_map_size=1024.0,
        ),
    )
    expected_script_fragments = (
        'getElement("42")',
        'document.getElementById("c42")',
        "child.isAmbientLight || child.isDirectionalLight",
        "child.userData.carveraManagedLight",
        "removeFromParent()",
        '"ambient": 0.2',
        '"key": 0.9',
        '"key_x": -100.0',
        '"shadow_map_size": 1024.0',
        "settings.ambient * Math.PI",
        "settings.key * Math.PI",
        "renderer.shadowMap.enabled",
        "key.castShadow",
        "key.shadow.camera.updateProjectionMatrix()",
        "toneMappingExposure = settings.exposure",
        'root.dataset.carveraLighting = "patched"',
    )
    for text in expected_script_fragments:
        assert text in script


def test_material_patch_updates_selected_scene_objects() -> None:
    material_script = scene_material_patch_javascript(
        42,
        [10, 11],
        ModelMaterialSettings(color="#eeeeee", opacity=0.5, roughness=0.25, metalness=0.75),
    )
    material_expected = (
        '"10"',
        '"11"',
        "material.opacity = settings.opacity",
        "material.roughness = settings.roughness",
        "material.metalness = settings.metalness",
        "child.castShadow = true",
        "child.receiveShadow = true",
        "window.setTimeout",
    )
    for text in material_expected:
        assert text in material_script
