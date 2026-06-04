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

from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from typing import Any

from nicegui import ui

from gui.scene.lighting import DEFAULT_MODEL_COLOR, ModelMaterialSettings, SceneLightingSettings

GUI_REALTIME_SPEED_MAX = 10.0


@dataclass
class EnvironmentTabView:
    motor_alarm_switches: dict[str, Any] = field(default_factory=dict)
    motor_alarm_badges: dict[str, Any] = field(default_factory=dict)
    spindle_alarm_switch: Any | None = None
    spindle_alarm_badge: Any | None = None
    lighting_controls: dict[str, Any] = field(default_factory=dict)
    material_controls: dict[str, Any] = field(default_factory=dict)
    realtime_speed: Any | None = None
    temperature_sensor: Any | None = None
    temperature_celsius: Any | None = None
    temperature_status: Any | None = None


def build_environment_tab(
    *,
    motor_alarm_changed: Callable[[str, Any], Awaitable[None]],
    spindle_alarm_changed: Callable[[Any], Awaitable[None]],
    realtime_speed_changed: Callable[[Any], Awaitable[None]],
    scene_appearance_changed: Callable[[Any], None],
    set_temperature: Callable[[], Awaitable[None]],
) -> EnvironmentTabView:
    view = EnvironmentTabView()
    with ui.element("div").classes("panel-section"):
        ui.label("Simulation Clock").classes("section-title")
        with ui.element("div").classes("speed-drive"):
            view.realtime_speed = ui.slider(
                min=0.25,
                max=GUI_REALTIME_SPEED_MAX,
                value=1.0,
                step=0.25,
            ).props("label-always")
            view.realtime_speed.on(
                "update:model-value",
                realtime_speed_changed,
                throttle=0.15,
                leading_events=True,
                trailing_events=True,
                js_handler="(value) => emit(value)",
            )
            ui.label("x requested").classes("metric-name")

    with ui.element("div").classes("panel-section"):
        ui.label("Fault Injection").classes("section-title")
        with ui.element("div").classes("input-grid"):
            for axis in ("X", "Y", "Z"):
                view.motor_alarm_switches[axis] = ui.switch(
                    f"{axis} motor alarm",
                    value=False,
                    on_change=lambda event, axis=axis: motor_alarm_changed(axis, event),
                )
                view.motor_alarm_badges[axis] = ui.label("clear").classes("badge-off")
            view.spindle_alarm_switch = ui.switch("Spindle alarm", value=False, on_change=spindle_alarm_changed)
            view.spindle_alarm_badge = ui.label("clear").classes("badge-off")

    with ui.element("div").classes("panel-section"):
        ui.label("Thermistors").classes("section-title")
        with ui.element("div").classes("temperature-drive"):
            view.temperature_sensor = ui.select(
                {"spindle": "Spindle", "power": "Power"}, value="spindle", label="Sensor"
            ).props("dense outlined")
            view.temperature_celsius = ui.slider(min=0, max=100, value=25.0, step=1.0).props("label-always")
            ui.button("Set Temp", on_click=set_temperature).props("dense color=primary")
        view.temperature_status = ui.label("--").classes("metric-value")

    with ui.element("div").classes("panel-section"):
        lighting_defaults = SceneLightingSettings()
        material_defaults = ModelMaterialSettings()
        ui.label("Scene Appearance").classes("section-title")
        with ui.element("div").classes("appearance-grid"):
            for name, label, value, minimum, maximum, step in (
                ("ambient", "Ambient", lighting_defaults.ambient, 0.0, 1.2, 0.01),
                ("key", "Key light", lighting_defaults.key, 0.0, 1.6, 0.01),
                ("fill", "Fill light", lighting_defaults.fill, 0.0, 1.0, 0.01),
                ("exposure", "Exposure", lighting_defaults.exposure, 0.2, 1.8, 0.01),
            ):
                with ui.element("div").classes("appearance-control"):
                    ui.label(label).classes("metric-name")
                    view.lighting_controls[name] = ui.slider(
                        min=minimum,
                        max=maximum,
                        value=value,
                        step=step,
                        on_change=scene_appearance_changed,
                    ).props("label-always")
            view.lighting_controls["shadows"] = ui.switch(
                "Shadows",
                value=lighting_defaults.shadows,
                on_change=scene_appearance_changed,
            )
            for name, label, value, minimum, maximum, step in (
                ("shadow_radius", "Shadow softness", lighting_defaults.shadow_radius, 0.0, 12.0, 0.25),
                ("shadow_bias", "Shadow bias", lighting_defaults.shadow_bias, -0.01, 0.01, 0.0001),
                ("shadow_map_size", "Shadow map", lighting_defaults.shadow_map_size, 512.0, 4096.0, 512.0),
                ("key_x", "Key X", lighting_defaults.key_x, -600.0, 600.0, 10.0),
                ("key_y", "Key Y", lighting_defaults.key_y, -600.0, 600.0, 10.0),
                ("key_z", "Key Z", lighting_defaults.key_z, 40.0, 900.0, 10.0),
                ("fill_x", "Fill X", lighting_defaults.fill_x, -600.0, 600.0, 10.0),
                ("fill_y", "Fill Y", lighting_defaults.fill_y, -600.0, 600.0, 10.0),
                ("fill_z", "Fill Z", lighting_defaults.fill_z, 40.0, 900.0, 10.0),
            ):
                with ui.element("div").classes("appearance-control"):
                    ui.label(label).classes("metric-name")
                    view.lighting_controls[name] = ui.slider(
                        min=minimum,
                        max=maximum,
                        value=value,
                        step=step,
                        on_change=scene_appearance_changed,
                    ).props("label-always")
            with ui.element("div").classes("appearance-control"):
                ui.label("Shell color").classes("metric-name")
                view.material_controls["color"] = ui.color_input(
                    value=DEFAULT_MODEL_COLOR,
                    on_change=scene_appearance_changed,
                ).props("dense outlined")
            for name, label, value, minimum, maximum, step in (
                ("opacity", "Opacity", material_defaults.opacity, 0.1, 1.0, 0.01),
                ("roughness", "Roughness", material_defaults.roughness, 0.0, 1.0, 0.01),
                ("metalness", "Metalness", material_defaults.metalness, 0.0, 1.0, 0.01),
            ):
                with ui.element("div").classes("appearance-control"):
                    ui.label(label).classes("metric-name")
                    view.material_controls[name] = ui.slider(
                        min=minimum,
                        max=maximum,
                        value=value,
                        step=step,
                        on_change=scene_appearance_changed,
                    ).props("label-always")

    return view
