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

from dataclasses import dataclass
from enum import StrEnum
from typing import Any

from gui.generated import carvera_sim_pb2 as pb


class ToolKind(StrEnum):
    UNSPECIFIED = "unspecified"
    CUTTING_TOOL = "cutting_tool"
    STOCK_Z_PROBE = "stock_z_probe"
    THREE_AXIS_PROBE = "three_axis_probe"


class EepromFieldType(StrEnum):
    BOOL = "bool"
    INT = "int"
    FLOAT = "float"


class MachineModel(StrEnum):
    CARVERA_C1 = "c1"
    CARVERA_AIR_CA1 = "ca1"


@dataclass(frozen=True, slots=True)
class Box3D:
    min_x: float
    min_y: float
    min_z: float
    max_x: float
    max_y: float
    max_z: float

    @property
    def center_x(self) -> float:
        return (self.min_x + self.max_x) / 2.0

    @property
    def center_y(self) -> float:
        return (self.min_y + self.max_y) / 2.0

    @property
    def center_z(self) -> float:
        return (self.min_z + self.max_z) / 2.0

    @property
    def top_z(self) -> float:
        return max(self.min_z, self.max_z)

    def stable_key(self) -> tuple[float, float, float, float, float, float]:
        return (
            round(self.min_x, 6),
            round(self.min_y, 6),
            round(self.min_z, 6),
            round(self.max_x, 6),
            round(self.max_y, 6),
            round(self.max_z, 6),
        )


@dataclass(frozen=True, slots=True)
class AxisSnapshot:
    axis: str
    physical_steps: int
    physical_mm: float
    machine_position: float
    endstop_triggered: bool


@dataclass(frozen=True, slots=True)
class SpindleSnapshot:
    spinning: bool
    actual_rpm: float
    target_rpm: float
    max_rpm: float


@dataclass(frozen=True, slots=True)
class ToolSnapshot:
    active_tool: int
    target_tool: int
    tool_offset_mm: float
    cur_tool_mz: float
    ref_tool_mz: float
    target_collet_type: int
    length_mm: float
    kind: ToolKind
    probe_tip_diameter_mm: float


@dataclass(frozen=True, slots=True)
class AtcPocketSnapshot:
    pocket: int
    tool: int
    occupied: bool
    length_mm: float
    x: float
    y: float
    z: float
    kind: ToolKind
    probe_tip_diameter_mm: float


@dataclass(frozen=True, slots=True)
class ToolConfig:
    pocket: int
    tool: int
    length_mm: float
    occupied: bool
    kind: ToolKind
    probe_tip_diameter_mm: float = 0.0


@dataclass(frozen=True, slots=True)
class AtcSnapshot:
    available: bool
    spindle: ToolSnapshot
    pockets: tuple[AtcPocketSnapshot, ...]

    def pocket(self, pocket_id: int) -> AtcPocketSnapshot:
        for pocket in self.pockets:
            if pocket.pocket == pocket_id:
                return pocket
        raise KeyError(pocket_id)

    def pockets_by_id(self) -> dict[int, AtcPocketSnapshot]:
        return {pocket.pocket: pocket for pocket in self.pockets}


@dataclass(frozen=True, slots=True)
class MachineState:
    firmware_booted: bool
    homed: bool
    soft_endstop_enabled: bool
    work_area: Box3D | None
    physical_travel: Box3D | None
    axes: tuple[AxisSnapshot, ...]
    atc: AtcSnapshot | None
    spindle: SpindleSnapshot | None
    tool_setter: Box3D | None
    telemetry_time_s: float | None = None

    def axes_by_name(self) -> dict[str, AxisSnapshot]:
        return {axis.axis: axis for axis in self.axes}

    def axis(self, name: str) -> AxisSnapshot:
        for axis in self.axes:
            if axis.axis == name:
                return axis
        raise KeyError(name)

    def axis_positions(self) -> dict[str, float]:
        return {axis.axis: axis.physical_mm for axis in self.axes}


@dataclass(frozen=True, slots=True)
class EepromField:
    name: str
    type: EepromFieldType
    value: bool | int | float


@dataclass(frozen=True, slots=True)
class SwitchOutputSnapshot:
    available: bool
    on: bool
    value: float


@dataclass(frozen=True, slots=True)
class PwmOutputSnapshot:
    configured: bool
    duty: float
    period_us: float


@dataclass(frozen=True, slots=True)
class PhysicalIoState:
    probe_contact: bool
    tool_setter_contact: bool
    cover_open: bool
    front_panel: Any
    motor_alarms: dict[str, bool]
    spindle_alarm_available: bool
    spindle_alarm_triggered: bool
    switches: dict[str, SwitchOutputSnapshot]
    laser: Any
    pwm_outputs: dict[tuple[int, int], PwmOutputSnapshot]


@dataclass(frozen=True, slots=True)
class TransportEndpoint:
    host: str
    port: int


@dataclass(frozen=True, slots=True)
class InteractiveTransportState:
    uart_supported: bool
    uart_path: str
    tcp_endpoints: tuple[TransportEndpoint, ...]


def snapshot_to_state(snapshot: pb.MachineSnapshot) -> MachineState:
    return MachineState(
        firmware_booted=bool(snapshot.firmware_booted),
        homed=bool(snapshot.homed),
        soft_endstop_enabled=bool(snapshot.soft_endstop_enabled),
        work_area=_box_to_state(snapshot.work_area),
        physical_travel=_box_to_state(snapshot.physical_travel),
        axes=_axes_to_state(snapshot.axes),
        atc=_atc_to_state(snapshot.atc),
        spindle=_spindle_to_state(snapshot.spindle),
        tool_setter=_box_to_state(snapshot.tool_setter) if snapshot.tool_setter_available else None,
    )


def telemetry_to_state(telemetry: pb.MachineTelemetry) -> MachineState:
    return MachineState(
        firmware_booted=bool(telemetry.firmware_booted),
        homed=bool(telemetry.homed),
        soft_endstop_enabled=False,
        work_area=None,
        physical_travel=_box_to_state(telemetry.physical_travel) if telemetry.HasField("physical_travel") else None,
        axes=_axes_to_state(telemetry.axes),
        atc=_atc_to_state(telemetry.atc) if telemetry.HasField("atc") else None,
        spindle=_spindle_to_state(telemetry.spindle) if telemetry.HasField("spindle") else None,
        tool_setter=None,
        telemetry_time_s=float(telemetry.time_us) / 1_000_000.0,
    )


def physical_io_to_state(snapshot: pb.PhysicalIoSnapshot) -> PhysicalIoState:
    return PhysicalIoState(
        probe_contact=bool(snapshot.probe_inputs.probe),
        tool_setter_contact=bool(snapshot.probe_inputs.tool_setter),
        cover_open=bool(snapshot.cover.open),
        front_panel=snapshot.front_panel,
        motor_alarms={_axis_name(alarm.axis): bool(alarm.triggered) for alarm in snapshot.motor_alarms},
        spindle_alarm_available=bool(snapshot.spindle_alarm.available),
        spindle_alarm_triggered=bool(snapshot.spindle_alarm.triggered),
        switches={
            _switch_name(state.name): SwitchOutputSnapshot(
                available=bool(state.available), on=bool(state.on), value=float(state.value)
            )
            for state in snapshot.switches
        },
        laser=snapshot.laser,
        pwm_outputs={
            (int(pwm.pin.port), int(pwm.pin.pin)): PwmOutputSnapshot(
                configured=bool(pwm.configured), duty=float(pwm.duty), period_us=float(pwm.period_us)
            )
            for pwm in snapshot.pwm_outputs
        },
    )


def eeprom_fields_to_state(fields: pb.EepromFields) -> list[EepromField]:
    rows: list[EepromField] = []
    for field in fields.fields:
        if field.type == pb.EEPROM_FIELD_TYPE_BOOL:
            rows.append(EepromField(name=field.name, type=EepromFieldType.BOOL, value=bool(field.boolean)))
        elif field.type == pb.EEPROM_FIELD_TYPE_INT:
            rows.append(EepromField(name=field.name, type=EepromFieldType.INT, value=int(field.integer)))
        else:
            rows.append(EepromField(name=field.name, type=EepromFieldType.FLOAT, value=float(field.number)))
    return rows


def interactive_transport_to_state(transport: pb.InteractiveTransport) -> InteractiveTransportState:
    return InteractiveTransportState(
        uart_supported=bool(transport.uart_supported),
        uart_path=str(transport.uart_path),
        tcp_endpoints=tuple(
            TransportEndpoint(host=str(endpoint.host), port=int(endpoint.port)) for endpoint in transport.tcp_endpoints
        ),
    )


def _box_to_state(box: pb.Box) -> Box3D:
    return Box3D(
        min_x=float(box.min_x),
        min_y=float(box.min_y),
        min_z=float(box.min_z),
        max_x=float(box.max_x),
        max_y=float(box.max_y),
        max_z=float(box.max_z),
    )


def _axes_to_state(axes: Any) -> tuple[AxisSnapshot, ...]:
    return tuple(
        AxisSnapshot(
            axis=_axis_name(axis.axis),
            physical_steps=int(axis.physical_steps),
            physical_mm=float(axis.physical_mm),
            machine_position=float(axis.machine_position),
            endstop_triggered=bool(axis.endstop_triggered),
        )
        for axis in axes
    )


def _axis_name(axis: int) -> str:
    return pb.Axis.Name(axis).replace("AXIS_", "")


def _switch_name(name: int) -> str:
    return pb.SwitchName.Name(name).replace("SWITCH_NAME_", "").lower()


def _spindle_to_state(spindle: pb.SpindleState) -> SpindleSnapshot:
    return SpindleSnapshot(
        spinning=bool(spindle.spinning),
        actual_rpm=float(spindle.actual_rpm),
        target_rpm=float(spindle.target_rpm),
        max_rpm=float(spindle.max_rpm),
    )


def _atc_to_state(atc: pb.AtcState) -> AtcSnapshot:
    return AtcSnapshot(
        available=bool(atc.available),
        spindle=_tool_to_state(atc.spindle),
        pockets=tuple(_pocket_to_state(pocket) for pocket in atc.pockets),
    )


def _tool_to_state(tool: pb.ToolState) -> ToolSnapshot:
    return ToolSnapshot(
        active_tool=int(tool.active_tool),
        target_tool=int(tool.target_tool),
        tool_offset_mm=float(tool.tool_offset_mm),
        cur_tool_mz=float(tool.cur_tool_mz),
        ref_tool_mz=float(tool.ref_tool_mz),
        target_collet_type=int(tool.target_collet_type),
        length_mm=float(tool.length_mm),
        kind=_tool_kind_name(tool.kind),
        probe_tip_diameter_mm=float(tool.probe_tip_diameter_mm),
    )


def _pocket_to_state(pocket: pb.AtcPocketTool) -> AtcPocketSnapshot:
    return AtcPocketSnapshot(
        pocket=int(pocket.pocket),
        tool=int(pocket.tool),
        occupied=bool(pocket.occupied),
        length_mm=float(pocket.length_mm),
        x=float(pocket.x),
        y=float(pocket.y),
        z=float(pocket.z),
        kind=_tool_kind_name(pocket.kind),
        probe_tip_diameter_mm=float(pocket.probe_tip_diameter_mm),
    )


def _tool_kind_name(kind: int) -> ToolKind:
    by_value: dict[int, ToolKind] = {
        pb.TOOL_KIND_CUTTING_TOOL: ToolKind.CUTTING_TOOL,
        pb.TOOL_KIND_STOCK_Z_PROBE: ToolKind.STOCK_Z_PROBE,
        pb.TOOL_KIND_THREE_AXIS_PROBE: ToolKind.THREE_AXIS_PROBE,
    }
    return by_value.get(int(kind), ToolKind.UNSPECIFIED)


def tool_kind_to_proto(kind: ToolKind | str) -> pb.ToolKind:
    try:
        tool_kind = ToolKind(str(kind))
    except ValueError:
        tool_kind = ToolKind.UNSPECIFIED
    return {
        ToolKind.UNSPECIFIED: pb.TOOL_KIND_UNSPECIFIED,
        ToolKind.CUTTING_TOOL: pb.TOOL_KIND_CUTTING_TOOL,
        ToolKind.STOCK_Z_PROBE: pb.TOOL_KIND_STOCK_Z_PROBE,
        ToolKind.THREE_AXIS_PROBE: pb.TOOL_KIND_THREE_AXIS_PROBE,
    }.get(tool_kind, pb.TOOL_KIND_UNSPECIFIED)
