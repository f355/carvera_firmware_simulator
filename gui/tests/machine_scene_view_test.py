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

from gui.core.defaults import C1_SPINDLE_FACE_LOCAL, CA1_SPINDLE_FACE_LOCAL
from gui.protocol.model import (
    AxisSnapshot,
    AtcSnapshot,
    Box3D,
    MachineState,
    SpindleSnapshot,
    ToolKind,
    ToolSnapshot,
)
from gui.scene.machine_model_asset import MachineModelAsset
from gui.scene.machine_scene_view import MachineSceneView, box_key, visible_shell_components
from gui.scene.scene_transform import (
    CA1_ETS_TOP_SCENE_Z,
    C1_ATC_RACK_TOP_SCENE_Z,
    C1_BED_MESH_Y_ALIGNMENT_MM,
    SceneTransform,
    ca1_bed_scene_point,
    ca1_spindle_face_scene_point,
    c1_model_point,
    c1_spindle_face_point,
)


class FakeObject:
    def __init__(self) -> None:
        self.last_move: tuple[float, float, float] | None = None
        self.visibility: list[bool] = []
        self.materials: list[object] = []
        self.deleted = False

    def move(self, x: float, y: float, z: float) -> "FakeObject":
        self.last_move = (x, y, z)
        return self

    def scale(self, _value: float) -> "FakeObject":
        return self

    def rotate(self, *_values: float) -> "FakeObject":
        return self

    def material(self, *values: object) -> "FakeObject":
        self.materials.append(values[0] if len(values) == 1 else values)
        return self

    def visible(self, value: bool) -> "FakeObject":
        self.visibility.append(value)
        return self

    def delete(self) -> None:
        self.deleted = True


class FakeLine:
    def __init__(self, start: list[float], end: list[float]) -> None:
        self.start = start
        self.end = end
        self.last_move: tuple[float, float, float] | None = None
        self.color = ""
        self.deleted = False

    def material(self, color: str) -> "FakeLine":
        self.color = color
        return self

    def move(self, x: float, y: float, z: float) -> None:
        self.last_move = (x, y, z)

    def delete(self) -> None:
        self.deleted = True


class FakeScene:
    def __init__(self) -> None:
        self.lines: list[FakeLine] = []
        self.objects: list[FakeObject] = []
        self.cylinders: list[FakeObject] = []
        self.cylinder_kwargs: list[dict[str, object]] = []
        self.boxes: list[FakeObject] = []

    def line(self, start: list[float], end: list[float]) -> FakeLine:
        line = FakeLine(start, end)
        self.lines.append(line)
        return line

    def gltf(self, _url: str) -> FakeObject:
        model = FakeObject()
        self.objects.append(model)
        return model

    def cylinder(self, **kwargs: object) -> FakeObject:
        model = FakeObject()
        self.cylinders.append(model)
        self.cylinder_kwargs.append(kwargs)
        return model

    def box(self, **_kwargs: object) -> FakeObject:
        model = FakeObject()
        self.boxes.append(model)
        return model


class FakeLabel:
    def __init__(self) -> None:
        self.text = ""

    def classes(self, **_kwargs: object) -> None:
        pass


class FakeAxisMode:
    def __init__(self, value: object = "3") -> None:
        self.value = value


def box(
    *,
    min_x: float,
    min_y: float,
    min_z: float,
    max_x: float,
    max_y: float,
    max_z: float,
) -> Box3D:
    return Box3D(min_x=min_x, min_y=min_y, min_z=min_z, max_x=max_x, max_y=max_y, max_z=max_z)


def axis(name: str, physical_mm: float) -> AxisSnapshot:
    return AxisSnapshot(
        axis=name,
        physical_steps=0,
        physical_mm=physical_mm,
        machine_position=physical_mm,
        endstop_triggered=False,
    )


def machine_state(
    *,
    physical_travel: Box3D | None = None,
    work_area: Box3D | None = None,
    axes: tuple[AxisSnapshot, ...] = (),
    atc: AtcSnapshot | None = None,
    spindle: SpindleSnapshot | None = None,
    tool_setter: Box3D | None = None,
    telemetry_time_s: float | None = None,
) -> MachineState:
    return MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=work_area,
        physical_travel=physical_travel,
        axes=axes,
        atc=atc,
        spindle=spindle,
        tool_setter=tool_setter,
        telemetry_time_s=telemetry_time_s,
    )


def tool(length_mm: float = 0.0) -> ToolSnapshot:
    return ToolSnapshot(
        active_tool=-1,
        target_tool=-1,
        tool_offset_mm=0.0,
        cur_tool_mz=0.0,
        ref_tool_mz=0.0,
        target_collet_type=0,
        length_mm=length_mm,
        kind=ToolKind.CUTTING_TOOL,
        probe_tip_diameter_mm=0.0,
    )


def split_components() -> dict[str, str]:
    return {
        "base": "/base.glb",
        "x": "/x.glb",
        "z": "/z.glb",
        "y3": "/y3.glb",
        "y4": "/y4.glb",
        "a_chuck": "/a_chuck.glb",
    }


def split_asset(
    *,
    label: str = "machine",
    machine_model: str | None = None,
    offset: tuple[float, float, float] = (0.0, 0.0, 0.0),
    spindle_face_local: tuple[float, float, float] | None = None,
) -> MachineModelAsset:
    return MachineModelAsset(
        url="/machine.glb",
        kind="gltf",
        label=label,
        machine_model=machine_model,
        components=split_components(),
        offset=offset,
        spindle_face_local=spindle_face_local,
    )


def ca1_asset() -> MachineModelAsset:
    return split_asset(
        label="CA1",
        machine_model="ca1",
        offset=(-82.5, -13.5, 33.0),
        spindle_face_local=CA1_SPINDLE_FACE_LOCAL,
    )


def c1_asset() -> MachineModelAsset:
    return split_asset(
        label="C1",
        machine_model="c1",
        offset=(-141.5, 13.0, 86.0),
        spindle_face_local=C1_SPINDLE_FACE_LOCAL,
    )


def make_view(
    *,
    scene: FakeScene | None,
    asset: MachineModelAsset | None,
    axis_mode: object = "3",
) -> MachineSceneView:
    return MachineSceneView(
        scene=scene,
        model_asset_for=lambda _model: asset,
        machine_model_label=FakeLabel(),
        axis_mode_select=FakeAxisMode(axis_mode),
        front_panel_badges={},
        axis_readouts={},
        axis_detail_rows={},
        machine_model_scale=1000.0,
    )


def rounded_tuple(values: tuple[float, ...] | list[float], places: int = 3) -> tuple[float, ...]:
    return tuple(round(value, places) for value in values)


def test_box_keys_and_visible_shell_components() -> None:
    key_box = box(min_x=1.0000004, min_y=2, min_z=3, max_x=4, max_y=5, max_z=6)
    assert box_key(key_box) == (1.0, 2, 3, 4, 5, 6)
    assert box_key(None) is None

    shell = MachineModelAsset(url="/machine.glb", kind="gltf", label="machine")
    split = split_asset()
    assert visible_shell_components(shell, "4") == {"shell"}
    assert visible_shell_components(split, "3") == {"base", "x", "z", "y3"}
    assert visible_shell_components(split, "4") == {"base", "x", "z", "y4", "a_chuck"}
    assert visible_shell_components(split, True) == {"base", "x", "z", "y4", "a_chuck"}


def test_shell_model_waits_for_simulator_geometry() -> None:
    split = split_asset()
    shell_scene = FakeScene()
    shell_view = make_view(scene=shell_scene, asset=split)
    shell_view.update_shell_model("ca1")
    assert shell_scene.objects
    assert not any(model.visibility[-1] for model in shell_scene.objects)

    shell_view.update(
        machine_state(
            physical_travel=box(min_x=-200.0, min_y=-150.0, min_z=-80.0, max_x=1.0, max_y=1.0, max_z=1.0),
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
        )
    )
    shown = {name for name, model in shell_view.machine_shell_objects.items() if model.visibility[-1]}
    assert shown == {"base", "x", "z", "y3"}

    shell_view.reset()
    assert not any(model.visibility[-1] for model in shell_scene.objects)


def test_fallback_spindle_and_bed_visibility() -> None:
    split = split_asset()
    fallback_scene = FakeScene()
    fallback_view = make_view(scene=fallback_scene, asset=split)
    fallback_view.update_shell_model("ca1")
    fallback_view.set_cad_models_visible(False)
    fallback_view.update(
        machine_state(
            physical_travel=box(min_x=-200.0, min_y=-150.0, min_z=-80.0, max_x=1.0, max_y=1.0, max_z=1.0),
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
        )
    )
    assert fallback_scene.cylinders
    assert fallback_scene.boxes
    assert fallback_scene.cylinder_kwargs[-1].get("radius") is None
    assert fallback_scene.cylinder_kwargs[-1]["top_radius"] == 8.6
    assert fallback_scene.cylinder_kwargs[-1]["bottom_radius"] == 8.6
    assert not any(model.visibility[-1] for model in fallback_scene.objects)
    assert fallback_scene.cylinders[-1].visibility[-1] is True
    assert fallback_scene.boxes[-1].visibility[-1] is True

    fallback_view.set_cad_models_visible(True)
    assert fallback_scene.cylinders[-1].visibility[-1] is True
    assert fallback_scene.boxes[-1].visibility[-1] is False


def test_spindle_overlay_uses_model_rpm_for_gradual_tint() -> None:
    split = split_asset()
    fallback_scene = FakeScene()
    fallback_view = make_view(scene=fallback_scene, asset=split)
    fallback_view.update_shell_model("ca1")
    fallback_view.update(
        machine_state(
            physical_travel=box(min_x=-200.0, min_y=-150.0, min_z=-80.0, max_x=1.0, max_y=1.0, max_z=1.0),
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
        )
    )
    fallback_view.update(machine_state(spindle=SpindleSnapshot(False, 14_500.0, 14_500.0, 14_500.0)))
    assert fallback_scene.cylinders[-1].materials[-1] == "#ff0000"

    fallback_view.update(machine_state(spindle=SpindleSnapshot(False, 5_000.0, 5_000.0, 0.0)))
    assert fallback_scene.cylinders[-1].materials[-1] != "#ff0000"

    fallback_view.update(machine_state(spindle=SpindleSnapshot(False, 10_000.0, 10_000.0, 15_500.0)))
    color_at_10k = fallback_scene.cylinders[-1].materials[-1]
    fallback_view.update(machine_state(spindle=SpindleSnapshot(False, 12_000.0, 12_000.0, 15_500.0)))
    assert fallback_scene.cylinders[-1].materials[-1] != color_at_10k


def test_c1_tool_setter_button_protrudes_above_rack_surface() -> None:
    split = split_asset()
    c1_ets_scene = FakeScene()
    c1_ets_view = make_view(scene=c1_ets_scene, asset=split)
    c1_ets_view.update_shell_model("c1")
    c1_trigger_z = -105.5
    c1_ets_view.update(
        machine_state(
            physical_travel=box(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0),
            tool_setter=box(
                min_x=-10.158,
                min_y=-60.568,
                min_z=c1_trigger_z - 2.0,
                max_x=1.842,
                max_y=-48.568,
                max_z=c1_trigger_z,
            ),
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
        )
    )
    assert c1_ets_scene.cylinder_kwargs[-1]["height"] == 8.0

    expected_button_center = c1_model_point(-4.158, -54.568, c1_trigger_z)
    expected_button_center[2] = C1_ATC_RACK_TOP_SCENE_Z + 4.0
    expected_button_center[1] += 2.0
    assert c1_ets_scene.cylinders[-1].last_move == tuple(expected_button_center)


def test_ca1_tool_setter_button_sits_above_modeled_ets_top() -> None:
    ca1_ets_scene = FakeScene()
    ca1_ets_view = make_view(scene=ca1_ets_scene, asset=ca1_asset())
    ca1_ets_view.update_shell_model("ca1")
    ca1_trigger_z = -115.0
    ca1_physical_travel = box(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=1.0, max_y=1.0, max_z=1.0)
    ca1_ets_view.update(
        machine_state(
            physical_travel=ca1_physical_travel,
            tool_setter=box(
                min_x=-17.0,
                min_y=-13.0,
                min_z=ca1_trigger_z - 2.0,
                max_x=-5.0,
                max_y=-1.0,
                max_z=ca1_trigger_z,
            ),
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
        )
    )
    assert ca1_ets_scene.cylinder_kwargs[-1]["height"] == 3.0

    ca1_transform = SceneTransform.from_work_area(ca1_physical_travel)
    ca1_bed_y_delta = -ca1_transform.point(-2.0, -2.0, -2.0)[1]
    expected_ca1_center = ca1_bed_scene_point(ca1_transform, -11.0, -7.0, ca1_trigger_z)
    expected_ca1_center[2] = CA1_ETS_TOP_SCENE_Z + 1.0 - 1.5
    expected_ca1_center[1] += ca1_bed_y_delta
    actual_ca1_center = ca1_ets_scene.cylinders[-1].last_move
    assert actual_ca1_center is not None
    assert rounded_tuple(actual_ca1_center) == rounded_tuple(expected_ca1_center)


def test_ca1_displayed_tool_tip_reaches_ets_below_button_top() -> None:
    ca1_ets_scene = FakeScene()
    ca1_ets_view = make_view(scene=ca1_ets_scene, asset=ca1_asset())
    ca1_ets_view.update_shell_model("ca1")
    ca1_trigger_z = -115.0
    tool_length = 58.0
    tool_stickout = tool_length - 20.0
    ca1_physical_travel = box(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=1.0, max_y=1.0, max_z=1.0)
    ca1_ets_view.update(
        machine_state(
            physical_travel=ca1_physical_travel,
            tool_setter=box(
                min_x=-17.0,
                min_y=-13.0,
                min_z=ca1_trigger_z - 2.0,
                max_x=-5.0,
                max_y=-1.0,
                max_z=ca1_trigger_z,
            ),
            axes=(axis("X", -11.0), axis("Y", -7.0), axis("Z", ca1_trigger_z + tool_stickout)),
            atc=AtcSnapshot(available=False, spindle=tool(tool_length), pockets=()),
        )
    )

    tool_setter_button = ca1_ets_scene.cylinders[-2]
    displayed_tool = ca1_ets_scene.cylinders[-1]
    button_top_z = (tool_setter_button.last_move or (0.0, 0.0, 0.0))[2] + 1.5
    tool_tip_z = (displayed_tool.last_move or (0.0, 0.0, 0.0))[2] - tool_stickout / 2.0
    assert round(button_top_z, 3) == 1.0
    assert round(tool_tip_z, 3) == 0.0


def test_ca1_spindle_face_offsets_the_work_envelope() -> None:
    asset = ca1_asset()
    ca1_view = make_view(scene=None, asset=asset)
    ca1_view.machine_shell_asset = asset
    ca1_physical = box(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=-1.0, max_y=-1.0, max_z=-1.0)
    ca1_view.scene_transform = SceneTransform.from_work_area(ca1_physical)
    ca1_homed_position = [-2.0, -2.0, -2.0]
    ca1_spindle = ca1_view._geometry().spindle_marker_position(
        ca1_homed_position,
        ca1_view.scene_transform.point(*ca1_homed_position),
    )
    ca1_uncalibrated_face_z = ca1_view.scene_transform.point(*ca1_homed_position)[2]
    assert round(ca1_spindle[2] - ca1_uncalibrated_face_z, 3) == -7.0
    ca1_scene = FakeScene()
    ca1_view.scene = ca1_scene
    ca1_view.work_envelope.scene = ca1_scene
    ca1_view.work_envelope.line_objects = ca1_view.work_envelope.draw_box_lines(
        ca1_physical, "#dc2626", ca1_view._geometry()
    )
    ca1_top_z = ca1_view._geometry().spindle_marker_position(
        [-1.0, -1.0, -1.0],
        ca1_view.scene_transform.point(-1.0, -1.0, -1.0),
    )[2]
    assert any(line.end[2] == ca1_top_z for line in ca1_scene.lines)


def test_c1_axis_components_keep_spindle_y_fixed_and_move_bed() -> None:
    asset = c1_asset()
    view = make_view(scene=None, asset=asset)
    view.machine_shell_asset = asset
    view.machine_shell_objects = {"z": FakeObject(), "y3": FakeObject()}
    physical = box(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
    view.scene_transform = SceneTransform.from_work_area(physical)
    c1_homed_position = [-1.0, -1.0, -1.0]
    c1_spindle = view._geometry().spindle_marker_position(
        c1_homed_position, view.scene_transform.point(*c1_homed_position)
    )
    c1_bed_z = c1_model_point(physical.min_x, physical.min_y, physical.min_z)[2]
    assert round(c1_spindle[2] - c1_bed_z, 3) == 153.0

    raw_position = [-2.0, -2.0, -2.0]
    scene_position = view.scene_transform.point(*raw_position)
    bed_y_delta = view._move_machine_axis_components(raw_position, scene_position)
    z_move = view.machine_shell_objects["z"].last_move
    if z_move is None:
        raise SystemExit("C1 Z component should move")
    spindle_face = (
        z_move[0] + C1_SPINDLE_FACE_LOCAL[0],
        z_move[1] + C1_SPINDLE_FACE_LOCAL[1],
        z_move[2] + C1_SPINDLE_FACE_LOCAL[2],
    )
    expected_face = tuple(c1_spindle_face_point(*raw_position))
    fixed_spindle_y = asset.offset[1] + C1_SPINDLE_FACE_LOCAL[1]
    assert rounded_tuple(spindle_face) == (
        round(expected_face[0], 3),
        round(fixed_spindle_y, 3),
        round(expected_face[2], 3),
    )
    assert view._geometry().spindle_marker_position(raw_position, scene_position) == [
        expected_face[0],
        fixed_spindle_y,
        expected_face[2],
    ]
    assert round(bed_y_delta, 3) == round(fixed_spindle_y - expected_face[1], 3)
    bed_move = view.machine_shell_objects["y3"].last_move
    if bed_move is None:
        raise SystemExit("C1 bed component should move")
    assert round(bed_move[1], 3) == round(asset.offset[1] + bed_y_delta + C1_BED_MESH_Y_ALIGNMENT_MM, 3)

    pocket_position = view._geometry().atc_rack_tool_position(-4.158, -234.568, -112.5, bed_y_delta)
    assert [round(value, 3) for value in pocket_position] == [
        172.5,
        round(-9.5 + bed_y_delta, 3),
        C1_ATC_RACK_TOP_SCENE_Z,
    ]


def test_c1_work_envelope_tracks_spindle_face_and_loaded_tool_tip() -> None:
    physical = box(min_x=-372.0, min_y=-251.0, min_z=-136.0, max_x=1.0, max_y=1.0, max_z=1.0)
    bed_y_delta = -7.5
    fake_scene = FakeScene()
    unloaded_view = make_view(scene=fake_scene, asset=None)
    unloaded_view.current_machine_model = "c1"
    unloaded_view.scene_transform = SceneTransform.from_work_area(physical)
    unloaded_view.work_envelope.line_objects = unloaded_view.work_envelope.draw_box_lines(
        physical, "#dc2626", unloaded_view._geometry()
    )
    assert any(line.end == c1_spindle_face_point(1.0, 1.0, 1.0) for line in fake_scene.lines)
    unloaded_view._move_work_area_lines(bed_y_delta)
    assert all(line.last_move == (0.0, bed_y_delta, 0.0) for line in fake_scene.lines)

    unloaded_view.latest_atc_data = AtcSnapshot(available=True, spindle=tool(70.0), pockets=())
    unloaded_view._move_work_area_lines(bed_y_delta)
    assert all(line.last_move == (0.0, bed_y_delta, -50.0) for line in fake_scene.lines)


def test_backplot_tracks_transformed_spindle_motion_and_can_be_cleared() -> None:
    physical = box(min_x=-303.0, min_y=-213.0, min_z=-122.0, max_x=1.0, max_y=1.0, max_z=1.0)
    scene = FakeScene()
    view = make_view(scene=scene, asset=ca1_asset())
    view.update_shell_model("ca1")

    view.update(
        machine_state(
            physical_travel=physical,
            axes=(axis("X", -2.0), axis("Y", -2.0), axis("Z", -2.0)),
            telemetry_time_s=10.0,
        ),
    )
    view.update(
        machine_state(
            physical_travel=physical,
            axes=(axis("X", -12.0), axis("Y", -2.0), axis("Z", -2.0)),
            telemetry_time_s=10.2,
        ),
    )

    backplot_line = scene.lines[-1]
    expected_start = ca1_spindle_face_scene_point(SceneTransform.from_work_area(physical), -2.0, -2.0, -2.0)
    expected_end = ca1_spindle_face_scene_point(SceneTransform.from_work_area(physical), -12.0, -2.0, -2.0)
    assert len(view.backplot.segments) == 1
    assert rounded_tuple(view.backplot.segments[0].start) == rounded_tuple(expected_start)
    assert rounded_tuple(view.backplot.segments[0].end) == rounded_tuple(expected_end)

    view.clear_backplot()
    assert backplot_line.deleted is True
    assert view.backplot.segments == []
