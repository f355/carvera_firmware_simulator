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

from dataclasses import dataclass, field
from math import radians
from typing import Any, Callable

from nicegui import ui

from gui.core.defaults import MACHINE_COMPONENT_COLORS, TOOL_SHANK_INSERT_MM
from gui.protocol.model import AtcSnapshot, Box3D, MachineState
from gui.scene.lighting import DEFAULT_MODEL_COLOR, ModelMaterialSettings, scene_material_patch_javascript

from .atc_tool_layer import AtcToolLayer
from .machine_model_asset import MachineModelAsset
from .scene_geometry import MachineSceneGeometry
from .scene_transform import (
    CA1_ETS_TOP_SCENE_Z,
    C1_ATC_RACK_TOP_SCENE_Z,
    SceneTransform,
)
from .spindle_overlay_layer import TOOL_ROTATION_X, SpindleOverlayLayer
from .work_envelope_layer import WorkEnvelopeLayer

FALLBACK_BED_THICKNESS_MM = 0.5
TOOL_SETTER_BUTTON_RADIUS_MM = 4.0
CA1_TOOL_SETTER_BUTTON_HEIGHT_MM = 3.0
C1_TOOL_SETTER_BUTTON_HEIGHT_MM = 8.0
TOOL_SETTER_TRIGGER_BELOW_TOP_MM = 1.0


@dataclass(frozen=True)
class ToolSetterVisualSpec:
    height_mm: float
    trigger_below_top_mm: float
    scene_top_z: float | None = None


def box_key(box: Box3D | None) -> tuple[float, ...] | None:
    if box is None:
        return None
    return box.stable_key()


def rotary_axis_connected(value: Any) -> bool:
    return value is True or str(value).lower() in {"4", "true", "on"}


def visible_shell_components(asset: MachineModelAsset, axis_mode: Any) -> set[str]:
    if not asset.components:
        return {"shell"}
    if rotary_axis_connected(axis_mode):
        return {"base", "x", "z", "y4", "a_chuck"}
    return {"base", "x", "z", "y3"}


def physical_tool_stickout_mm(atc: AtcSnapshot | None) -> float:
    if not atc:
        return 0.0
    return max(0.0, atc.spindle.length_mm - TOOL_SHANK_INSERT_MM)


@dataclass
class MachineSceneView:
    scene: Any
    model_asset_for: Callable[[str], MachineModelAsset | None]
    machine_model_label: Any
    axis_mode_select: Any
    front_panel_badges: dict[str, Any]
    axis_readouts: dict[str, Any]
    axis_detail_rows: dict[str, list[Any]]
    machine_model_scale: float
    model_offset_override: tuple[float, float, float] | None = None
    model_rotation_override: tuple[float, float, float] | None = None

    latest_work_area: Box3D | None = None
    latest_physical_travel: Box3D | None = None
    latest_atc_data: AtcSnapshot | None = None
    latest_tool_setter: Box3D | None = None
    scene_transform: SceneTransform | None = None
    current_machine_model: str | None = None
    machine_shell_asset: MachineModelAsset | None = None
    machine_shell_objects: dict[str, Any] = field(default_factory=dict)
    machine_shell_key: tuple[str, str] | None = None
    cad_models_visible: bool = True
    work_envelope: WorkEnvelopeLayer = field(init=False)
    spindle_overlay: SpindleOverlayLayer = field(init=False)
    fallback_bed: Any = None
    fallback_bed_base_position: tuple[float, float, float] | None = None
    tool_setter_button: Any = None
    tool_setter_button_height: float | None = None
    atc_tools: AtcToolLayer = field(init=False)
    material_settings: ModelMaterialSettings = field(default_factory=ModelMaterialSettings)

    def __post_init__(self) -> None:
        self.work_envelope = WorkEnvelopeLayer(scene=self.scene)
        self.spindle_overlay = SpindleOverlayLayer(scene=self.scene)
        self.atc_tools = AtcToolLayer(scene=self.scene)

    def reset(self) -> None:
        self.work_envelope.clear()
        self.latest_work_area = None
        self.latest_physical_travel = None
        self.latest_atc_data = None
        self.latest_tool_setter = None
        self.scene_transform = None
        self._apply_shell_visibility()
        self._apply_fallback_visibility()
        self.spindle_overlay.reset()
        self.atc_tools.reset()
        if self.tool_setter_button is not None:
            self.tool_setter_button.visible(False)

    def _tool_setter_visual_spec(self) -> ToolSetterVisualSpec:
        if self._is_c1_model():
            return ToolSetterVisualSpec(
                height_mm=C1_TOOL_SETTER_BUTTON_HEIGHT_MM,
                trigger_below_top_mm=TOOL_SETTER_TRIGGER_BELOW_TOP_MM,
                scene_top_z=C1_ATC_RACK_TOP_SCENE_Z + C1_TOOL_SETTER_BUTTON_HEIGHT_MM,
            )
        return ToolSetterVisualSpec(
            height_mm=CA1_TOOL_SETTER_BUTTON_HEIGHT_MM,
            trigger_below_top_mm=TOOL_SETTER_TRIGGER_BELOW_TOP_MM,
            scene_top_z=CA1_ETS_TOP_SCENE_Z + TOOL_SETTER_TRIGGER_BELOW_TOP_MM,
        )

    def update_shell_model(self, machine_model: str) -> None:
        self.current_machine_model = machine_model
        self._update_front_panel_led_label(machine_model)
        try:
            asset = self.model_asset_for(machine_model)
        except Exception as exc:
            self.machine_model_label.text = f"error: {exc}"
            self.machine_model_label.classes(remove="badge-on badge-off badge-warn", add="badge-warn")
            return

        if asset is None:
            self._clear_shell_model()
            return

        components = asset.components or {"shell": asset.url}
        key = (asset.kind, "|".join(f"{name}:{url}" for name, url in sorted(components.items())))
        if self.machine_shell_key != key:
            self._replace_shell_objects(asset, components)
            self.machine_shell_key = key

        self.machine_shell_asset = asset
        rotation = self.model_rotation_override or tuple(radians(value) for value in asset.rotation_degrees)
        offset = self.model_offset_override or asset.offset
        for name, model_object in self.machine_shell_objects.items():
            model_object.scale(self.machine_model_scale)
            model_object.move(*offset)
            model_object.rotate(*rotation)
        self._apply_shell_visibility()
        axis_label = "4-axis" if rotary_axis_connected(self.axis_mode_select.value) else "3-axis"
        self.machine_model_label.text = f"{asset.label} ({axis_label})" if asset.components else asset.label
        self.machine_model_label.classes(remove="badge-on badge-off badge-warn", add="badge-on")
        self.update_axis_mode_visibility()

    def set_cad_models_visible(self, visible: bool) -> None:
        self.cad_models_visible = visible
        self._apply_shell_visibility()
        self._apply_fallback_visibility()

    def update_axis_mode_visibility(self) -> None:
        show_a = rotary_axis_connected(self.axis_mode_select.value)
        if "A" in self.axis_readouts:
            self.axis_readouts["A"].set_visibility(show_a)
        self._apply_shell_visibility()

    def update(self, state: MachineState | None) -> None:
        if state is None:
            self.reset()
            return

        if state.work_area is not None:
            self.latest_work_area = state.work_area
        if state.physical_travel is not None:
            self.latest_physical_travel = state.physical_travel
        if state.atc is not None:
            self.latest_atc_data = state.atc
        if state.spindle is not None:
            self.spindle_overlay.update_spindle(state.spindle, self.current_machine_model)
        if state.tool_setter is not None:
            self.latest_tool_setter = state.tool_setter

        if not state.axes:
            return

        physical_box = state.physical_travel or self.latest_physical_travel or state.work_area or self.latest_work_area
        soft_box = state.work_area or self.latest_work_area
        key = (box_key(physical_box), box_key(soft_box))
        if physical_box is not None and key != self.work_envelope.key:
            self._redraw_work_area(physical_box, soft_box, key)

        axes = state.axis_positions()
        raw_position = [axes.get("X", 0.0), axes.get("Y", 0.0), axes.get("Z", 0.0)]
        if self.scene_transform is not None:
            position = self.scene_transform.point(axes.get("X", 0.0), axes.get("Y", 0.0), axes.get("Z", 0.0))
        else:
            position = raw_position
        bed_y_delta = self._move_machine_axis_components(raw_position, position)
        self._rotate_rotary_chuck(axes.get("A", 0.0))
        self._move_work_area_lines(bed_y_delta)
        self._move_fallback_primitives(raw_position, position, bed_y_delta)
        self._move_tool_setter_button(bed_y_delta)
        self._update_atc_tools(state, position, bed_y_delta)

    def _update_front_panel_led_label(self, machine_model: str) -> None:
        if "led_name" not in self.front_panel_badges or "rgb" not in self.front_panel_badges:
            return
        if machine_model == "ca1":
            self.front_panel_badges["led_name"].text = "CA1 LED strip"
            self.front_panel_badges["rgb"].text = "not decoded"
        else:
            self.front_panel_badges["led_name"].text = "C1 RGB LED"
            self.front_panel_badges["rgb"].text = "not available"

    def _clear_shell_model(self) -> None:
        for model_object in self.machine_shell_objects.values():
            model_object.delete()
        self.machine_shell_objects.clear()
        self.machine_shell_key = None
        self.machine_shell_asset = None
        self.machine_model_label.text = "none"
        self.machine_model_label.classes(remove="badge-on badge-off badge-warn", add="badge-off")

    def _replace_shell_objects(self, asset: MachineModelAsset, components: dict[str, str]) -> None:
        for model_object in self.machine_shell_objects.values():
            model_object.delete()
        self.machine_shell_objects.clear()
        for name, url in components.items():
            model_object = self.scene.stl(url) if asset.kind == "stl" else self.scene.gltf(url)
            model_object.material(self._component_material_color(name), self.material_settings.opacity, "both")
            model_object.visible(False)
            self.machine_shell_objects[name] = model_object
        self.apply_model_material(self.material_settings)

    def _component_material_color(self, name: str) -> str:
        if self.material_settings.color.lower() != DEFAULT_MODEL_COLOR:
            return self.material_settings.color
        return MACHINE_COMPONENT_COLORS.get(name, "#b8c2cc")

    def apply_model_material(self, settings: ModelMaterialSettings) -> None:
        self.material_settings = settings
        for name, model_object in self.machine_shell_objects.items():
            model_object.material(self._component_material_color(name), settings.opacity, "both")
        object_ids = [
            model_object.id for model_object in self.machine_shell_objects.values() if hasattr(model_object, "id")
        ]
        if self.scene is not None and hasattr(self.scene, "id") and object_ids:
            ui.run_javascript(
                scene_material_patch_javascript(
                    self.scene.id,
                    object_ids,
                    settings,
                )
            )

    def _apply_shell_visibility(self) -> None:
        if self.machine_shell_asset is None:
            return
        visible_components = (
            visible_shell_components(self.machine_shell_asset, self.axis_mode_select.value)
            if self.scene_transform is not None and self.cad_models_visible
            else set()
        )
        for name, model_object in self.machine_shell_objects.items():
            model_object.visible(name in visible_components)

    def _apply_fallback_visibility(self) -> None:
        self.spindle_overlay.set_visible(self.scene_transform is not None)
        if self.fallback_bed is not None:
            self.fallback_bed.visible(not self.cad_models_visible and self.scene_transform is not None)

    def _is_ca1_split_model(self) -> bool:
        return self._geometry().is_ca1_split_model

    def _is_c1_split_model(self) -> bool:
        return self._geometry().is_c1_split_model

    def _is_c1_model(self) -> bool:
        return self._geometry().is_c1_model

    def _uses_bed_aligned_motion(self) -> bool:
        return self._geometry().uses_bed_aligned_motion

    def _geometry(self) -> MachineSceneGeometry:
        asset = self.machine_shell_asset
        return MachineSceneGeometry(
            machine_model=self.current_machine_model,
            asset_machine_model=asset.machine_model if asset is not None else None,
            has_split_components=bool(asset.components) if asset is not None else False,
            transform=self.scene_transform,
            model_offset=self.model_offset_override or (asset.offset if asset is not None else (0.0, 0.0, 0.0)),
            spindle_face_local=(
                asset.spindle_face_local if asset is not None and asset.spindle_face_local else (0.0, 0.0, 0.0)
            ),
        )

    def _move_machine_axis_components(self, raw_position: list[float], scene_position: list[float]) -> float:
        if self.machine_shell_asset is None or not self.machine_shell_asset.components:
            return 0.0
        component_positions = self._geometry().axis_component_positions(raw_position, scene_position)
        for name, position in component_positions.positions.items():
            if name in self.machine_shell_objects:
                self.machine_shell_objects[name].move(*position)
        return component_positions.bed_y_delta

    def _rotate_rotary_chuck(self, angle_degrees: float) -> None:
        chuck = self.machine_shell_objects.get("a_chuck")
        if chuck is None or self.machine_shell_asset is None:
            return
        rotation = self.model_rotation_override or tuple(
            radians(value) for value in self.machine_shell_asset.rotation_degrees
        )
        # A is a rotary axis around the accessory's local X axis.
        chuck.rotate(rotation[0] + radians(angle_degrees), rotation[1], rotation[2])

    def _move_work_area_lines(self, y_delta: float) -> None:
        z_delta = -physical_tool_stickout_mm(self.latest_atc_data)
        self.work_envelope.move(y_delta=y_delta, z_delta=z_delta)

    def _redraw_work_area(
        self,
        physical_box: Box3D,
        soft_box: Box3D | None,
        key: tuple[tuple[float, ...] | None, tuple[float, ...] | None],
    ) -> None:
        self.scene_transform = SceneTransform.from_work_area(physical_box)
        self._apply_shell_visibility()
        self.work_envelope.scene = self.scene
        self.work_envelope.redraw(physical_box, soft_box, key=key, geometry=self._geometry())
        self._redraw_fallback_bed(physical_box)
        self._apply_fallback_visibility()

    def _redraw_fallback_bed(self, physical_box: Box3D) -> None:
        if self.scene_transform is None or self.scene is None:
            return
        if self.fallback_bed is not None:
            self.fallback_bed.delete()
            self.fallback_bed = None

        min_x, max_x = sorted((physical_box.min_x, physical_box.max_x))
        min_y, max_y = sorted((physical_box.min_y, physical_box.max_y))
        min_z = min(physical_box.min_z, physical_box.max_z)
        geometry = self._geometry()
        corner_min = geometry.scene_point(min_x, min_y, min_z)
        corner_max = geometry.scene_point(max_x, max_y, min_z)
        width = abs(corner_max[0] - corner_min[0])
        height = abs(corner_max[1] - corner_min[1])
        center = (
            (corner_min[0] + corner_max[0]) / 2.0,
            (corner_min[1] + corner_max[1]) / 2.0,
            corner_min[2] - FALLBACK_BED_THICKNESS_MM / 2.0,
        )
        self.fallback_bed_base_position = center
        self.fallback_bed = self.scene.box(width=width, height=height, depth=FALLBACK_BED_THICKNESS_MM).material(
            "#64748b", 0.22, "both"
        )
        self.fallback_bed.move(*center)
        self.fallback_bed.visible(False)

    def _move_fallback_primitives(
        self, raw_position: list[float], scene_position: list[float], bed_y_delta: float
    ) -> None:
        if self.scene_transform is None:
            return
        self.spindle_overlay.scene = self.scene
        spindle_position = self._geometry().spindle_marker_position(raw_position, scene_position)
        self.spindle_overlay.move_overlay(spindle_position, visible=self.scene_transform is not None)
        if self.fallback_bed is not None and self.fallback_bed_base_position is not None:
            self.fallback_bed.move(
                self.fallback_bed_base_position[0],
                self.fallback_bed_base_position[1] + bed_y_delta,
                self.fallback_bed_base_position[2],
            )
        self._apply_fallback_visibility()

    def _ensure_tool_setter_button(self) -> None:
        if self.scene is None:
            return
        height = self._tool_setter_visual_spec().height_mm
        if self.tool_setter_button is not None and self.tool_setter_button_height == height:
            return
        if self.tool_setter_button is not None:
            self.tool_setter_button.delete()
        self.tool_setter_button = (
            self.scene.cylinder(
                top_radius=TOOL_SETTER_BUTTON_RADIUS_MM,
                bottom_radius=TOOL_SETTER_BUTTON_RADIUS_MM,
                height=height,
                radial_segments=32,
            )
            .material("#111827")
            .rotate(TOOL_ROTATION_X, 0.0, 0.0)
        )
        self.tool_setter_button_height = height
        self.tool_setter_button.visible(False)

    def _move_tool_setter_button(self, bed_y_delta: float) -> None:
        if self.scene_transform is None or self.latest_tool_setter is None:
            if self.tool_setter_button is not None:
                self.tool_setter_button.visible(False)
            return
        self._ensure_tool_setter_button()
        if self.tool_setter_button is None:
            return

        box = self.latest_tool_setter
        center_x = box.center_x
        center_y = box.center_y
        trigger_z = box.top_z
        visual = self._tool_setter_visual_spec()
        height = visual.height_mm
        top_z = trigger_z + visual.trigger_below_top_mm
        position = self._geometry().scene_point(center_x, center_y, top_z - height / 2.0)
        if visual.scene_top_z is not None:
            position[2] = visual.scene_top_z - height / 2.0
        if self._uses_bed_aligned_motion() or self._is_c1_model():
            position[1] += bed_y_delta
        self.tool_setter_button.move(*position).visible(True)

    def _update_atc_tools(self, state: MachineState, position: list[float], bed_y_delta: float) -> None:
        axes = state.axis_positions()
        raw_position = [axes.get("X", 0.0), axes.get("Y", 0.0), axes.get("Z", 0.0)]
        geometry = self._geometry()
        spindle_position = geometry.spindle_marker_position(raw_position, position)
        atc = state.atc or self.latest_atc_data
        self.atc_tools.scene = self.scene
        self.atc_tools.update(atc, geometry=geometry, bed_y_delta=bed_y_delta)

        held_length = atc.spindle.length_mm if atc is not None else 0.0
        if held_length > 0.0:
            self.spindle_overlay.scene = self.scene
            self.spindle_overlay.show_tool(spindle_position, max(5.0, held_length - TOOL_SHANK_INSERT_MM))
        else:
            self.spindle_overlay.hide_tool()
