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
from dataclasses import asdict, dataclass
from typing import Any

from nicegui import ui


DEFAULT_MODEL_COLOR = "#d9dee4"


@dataclass(frozen=True)
class SceneLightingSettings:
    ambient: float = 0.2
    key: float = 0.5
    key_x: float = -50.0
    key_y: float = -250.0
    key_z: float = 250.0
    fill: float = 0.3
    fill_x: float = 40.0
    fill_y: float = 170.0
    fill_z: float = 160.0
    exposure: float = 0.88
    shadows: bool = True
    shadow_radius: float = 3.0
    shadow_bias: float = -0.003
    shadow_map_size: float = 1536.0


@dataclass(frozen=True)
class ModelMaterialSettings:
    color: str = DEFAULT_MODEL_COLOR
    opacity: float = 1.0
    roughness: float = 0.45
    metalness: float = 0.1


def scene_lighting_patch_javascript(scene_id: int, settings: SceneLightingSettings | None = None) -> str:
    payload = json.dumps(asdict(settings or SceneLightingSettings()))
    return f"""
(() => {{
  const settings = {payload};
  const root = document.getElementById("c{scene_id}");
  const element = typeof getElement === "function" ? getElement("{scene_id}") : null;
  if (!element || !element.scene || !element.renderer) {{
    if (root) {{
      root.dataset.carveraLighting = "missing-scene";
    }}
    return;
  }}

  const scene = element.scene;
  const defaultLights = scene.children.filter(
    (child) => child.userData.carveraManagedLight || child.isAmbientLight || child.isDirectionalLight,
  );
  const AmbientLight = defaultLights.find((child) => child.isAmbientLight)?.constructor;
  const DirectionalLight = defaultLights.find((child) => child.isDirectionalLight)?.constructor;
  if (!AmbientLight || !DirectionalLight) {{
    if (root) {{
      root.dataset.carveraLighting = "missing-light-constructors";
    }}
    return;
  }}

  defaultLights.forEach((child) => child.removeFromParent());

  element.renderer.shadowMap.enabled = Boolean(settings.shadows);
  if (element.renderer.shadowMap.enabled) {{
    element.renderer.shadowMap.type = 2;
  }}

  const ambient = new AmbientLight(0xffffff, settings.ambient * Math.PI);
  ambient.userData.carveraManagedLight = true;
  scene.add(ambient);

  const key = new DirectionalLight(0xffffff, settings.key * Math.PI);
  key.userData.carveraManagedLight = true;
  key.position.set(settings.key_x, settings.key_y, settings.key_z);
  key.castShadow = Boolean(settings.shadows);
  if (key.shadow) {{
    const mapSize = Math.max(256, Math.round(settings.shadow_map_size));
    key.shadow.mapSize.width = mapSize;
    key.shadow.mapSize.height = mapSize;
    key.shadow.radius = settings.shadow_radius;
    key.shadow.bias = settings.shadow_bias;
    key.shadow.camera.near = 1;
    key.shadow.camera.far = 1400;
    key.shadow.camera.left = -650;
    key.shadow.camera.right = 650;
    key.shadow.camera.top = 650;
    key.shadow.camera.bottom = -650;
    key.shadow.camera.updateProjectionMatrix();
  }}
  scene.add(key);

  const fill = new DirectionalLight(0xffffff, settings.fill * Math.PI);
  fill.userData.carveraManagedLight = true;
  fill.position.set(settings.fill_x, settings.fill_y, settings.fill_z);
  scene.add(fill);

  element.renderer.toneMappingExposure = settings.exposure;
  if (root) {{
    root.dataset.carveraLighting = "patched";
  }}
}})();
"""


def scene_material_patch_javascript(
    scene_id: int,
    object_ids: list[int],
    settings: ModelMaterialSettings,
    *,
    attempts: int = 10,
) -> str:
    payload = json.dumps(asdict(settings))
    ids = json.dumps([str(object_id) for object_id in object_ids])
    return f"""
(() => {{
  const settings = {payload};
  const objectIds = {ids};
  const element = typeof getElement === "function" ? getElement("{scene_id}") : null;
  if (!element || !element.objects) {{
    return;
  }}

  const apply = (remainingAttempts) => {{
    let touched = false;
    for (const objectId of objectIds) {{
      const object = element.objects.get(objectId);
      if (!object) {{
        continue;
      }}
      object.traverse((child) => {{
        if (!child.isMesh || !child.material) {{
          return;
        }}
        for (const material of Array.isArray(child.material) ? child.material : [child.material]) {{
          material.transparent = settings.opacity < 1.0;
          material.opacity = settings.opacity;
          if ("roughness" in material) {{
            material.roughness = settings.roughness;
          }}
          if ("metalness" in material) {{
            material.metalness = settings.metalness;
          }}
          material.needsUpdate = true;
          child.castShadow = true;
          child.receiveShadow = true;
          touched = true;
        }}
      }});
    }}
    if (!touched && remainingAttempts > 0) {{
      window.setTimeout(() => apply(remainingAttempts - 1), 150);
    }}
  }};
  apply({attempts});
}})();
"""


def apply_scene_lighting(scene: Any, settings: SceneLightingSettings | None = None) -> None:
    ui.run_javascript(scene_lighting_patch_javascript(scene.id, settings))


def configure_scene_lighting(scene: Any, settings: SceneLightingSettings | None = None) -> None:
    scene.on("init", lambda: apply_scene_lighting(scene, settings))
