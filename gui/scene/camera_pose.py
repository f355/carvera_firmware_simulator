# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.

from __future__ import annotations

import json
from typing import Any

from nicegui import ui

from gui.core.simulator_settings import CameraPose


async def read_camera_pose(scene: Any) -> CameraPose | None:
    value = await ui.run_javascript(
        f"""
(() => {{
  const element = typeof getElement === "function" ? getElement("{scene.id}") : null;
  if (!element?.camera || !element?.controls) return null;
  const camera = element.camera;
  const target = element.controls.target ?? element.look_at;
  return {{
    x: camera.position.x, y: camera.position.y, z: camera.position.z,
    look_at_x: target.x, look_at_y: target.y, look_at_z: target.z,
    up_x: camera.up.x, up_y: camera.up.y, up_z: camera.up.z,
    zoom: camera.zoom ?? 1,
  }};
}})()
""",
        timeout=2.0,
    )
    return CameraPose.from_dict(value)


def restore_camera_pose(scene: Any, pose: CameraPose) -> None:
    scene.move_camera(
        x=pose.x,
        y=pose.y,
        z=pose.z,
        look_at_x=pose.look_at_x,
        look_at_y=pose.look_at_y,
        look_at_z=pose.look_at_z,
        up_x=pose.up_x,
        up_y=pose.up_y,
        up_z=pose.up_z,
        duration=0,
    )
    zoom = json.dumps(pose.zoom)
    ui.run_javascript(
        f"""
(() => {{
  const element = typeof getElement === "function" ? getElement("{scene.id}") : null;
  if (!element?.camera) return;
  element.camera.zoom = {zoom};
  element.camera.updateProjectionMatrix();
  element.controls?.update();
}})()
"""
    )
