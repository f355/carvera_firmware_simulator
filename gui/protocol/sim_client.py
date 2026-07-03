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
from typing import Any, Callable

from gui.protocol.client_error import SimulatorClientError as SimulatorClientError
from gui.protocol.framed_transport import FramedTransport
from gui.protocol.simulator_process import SimulatorProcess

from .proto_codegen import add_generated_to_path
from .model import (
    EepromField,
    EepromFieldType,
    InteractiveTransportState,
    ToolConfig,
    ToolKind,
    eeprom_fields_to_state,
    interactive_transport_to_state,
    tool_kind_to_proto,
)


add_generated_to_path()
import carvera_sim_pb2 as pb  # noqa: E402


AXIS_TO_PROTO = {
    "X": pb.AXIS_X,
    "Y": pb.AXIS_Y,
    "Z": pb.AXIS_Z,
    "A": pb.AXIS_A,
    "B": pb.AXIS_B,
}
LIMIT_SIDE_TO_PROTO = {
    "min": pb.LIMIT_SWITCH_SIDE_MIN,
    "max": pb.LIMIT_SWITCH_SIDE_MAX,
}
TEMPERATURE_SENSOR_TO_PROTO = {
    "spindle": pb.TEMPERATURE_SENSOR_SPINDLE,
    "power": pb.TEMPERATURE_SENSOR_POWER,
}
SWITCH_NAME_TO_PROTO = {
    "light": pb.SWITCH_NAME_LIGHT,
    "vacuum": pb.SWITCH_NAME_VACUUM,
    "spindle_fan": pb.SWITCH_NAME_SPINDLE_FAN,
    "power_fan": pb.SWITCH_NAME_POWER_FAN,
    "tool_sensor": pb.SWITCH_NAME_TOOL_SENSOR,
    "probe_charger": pb.SWITCH_NAME_PROBE_CHARGER,
    "air": pb.SWITCH_NAME_AIR,
    "beep": pb.SWITCH_NAME_BEEP,
}
BoxTuple = tuple[float, float, float, float, float, float]
DEFAULT_FUNCTION_SETTING = 0x02


class SimulatorClient:
    def __init__(
        self,
        binary: Path,
        *,
        stream_frames: bool = False,
        event_handler: Callable[[Any], None] | None = None,
        stderr_handler: Callable[[str], None] | None = None,
        inherit_stderr: bool = False,
        request_timeout_s: float = 10.0,
    ) -> None:
        self.process = SimulatorProcess(
            binary,
            stderr_handler=stderr_handler,
            inherit_stderr=inherit_stderr,
        )
        self.transport = FramedTransport(
            self.process,
            stream_frames=stream_frames,
            event_handler=event_handler,
            request_timeout_s=request_timeout_s,
        )

    @property
    def powered(self) -> bool:
        return self.transport.powered

    def start(self) -> None:
        self.transport.start()

    def stop(self) -> None:
        self.transport.stop()

    def request(self, request: pb.Request) -> pb.Response:
        return self.transport.request(request)

    def mount_filesystem(self, name: str, host_path: Path) -> None:
        request = pb.Request()
        request.mount_filesystem.name = name
        request.mount_filesystem.host_path = str(host_path)
        self.request(request)

    def set_machine_model(self, model: str) -> None:
        request = pb.Request()
        if model == "ca1":
            request.set_machine_model.machine_model = pb.MACHINE_MODEL_CARVERA_AIR_CA1
        else:
            request.set_machine_model.machine_model = pb.MACHINE_MODEL_CARVERA_C1
        request.set_machine_model.function_setting = DEFAULT_FUNCTION_SETTING
        self.request(request)

    def set_rotary_accessory_installed(self, installed: bool) -> None:
        request = pb.Request()
        request.set_rotary_accessory_installed.installed = installed
        self.request(request)

    def set_realtime(self) -> None:
        request = pb.Request()
        request.set_time_mode.mode = pb.TIME_MODE_REALTIME
        self.request(request)

    def set_realtime_speed(self, multiplier: float) -> None:
        request = pb.Request()
        request.set_realtime_speed.multiplier = multiplier
        self.request(request)

    def get_status(self) -> Any:
        request = pb.Request()
        request.get_status.SetInParent()
        return self.request(request).status

    def get_gpio_level(self, port: int, pin: int) -> bool:
        request = pb.Request()
        request.get_gpio_level.pin.port = port
        request.get_gpio_level.pin.pin = pin
        return bool(self.request(request).gpio_level.high)

    def get_probe_inputs(self) -> Any:
        request = pb.Request()
        request.get_probe_inputs.SetInParent()
        return self.request(request).probe_inputs

    def set_probe_tool_installed(self, installed: bool) -> None:
        request = pb.Request()
        request.set_probe_tool_installed.installed = installed
        self.request(request)

    def set_stock_box(self, box: BoxTuple, *, enabled: bool = True) -> None:
        request = pb.Request()
        self._fill_box(request.set_stock_box.bounds, box)
        request.set_stock_box.enabled = enabled
        self.request(request)

    def set_cover_open(self, open_: bool) -> None:
        request = pb.Request()
        request.set_cover_open.open = open_
        self.request(request)

    def get_cover_open(self) -> bool:
        request = pb.Request()
        request.get_cover_open.SetInParent()
        return bool(self.request(request).cover_state.open)

    def set_motor_alarm(self, axis: str, triggered: bool) -> None:
        request = pb.Request()
        request.set_motor_alarm.axis = AXIS_TO_PROTO[axis]
        request.set_motor_alarm.triggered = triggered
        self.request(request)

    def get_motor_alarm(self, axis: str) -> bool:
        request = pb.Request()
        request.get_motor_alarm.axis = AXIS_TO_PROTO[axis]
        return bool(self.request(request).motor_alarm_state.triggered)

    def set_spindle_alarm(self, triggered: bool) -> None:
        request = pb.Request()
        request.set_spindle_alarm.triggered = triggered
        self.request(request)

    def get_spindle_alarm(self) -> Any:
        request = pb.Request()
        request.get_spindle_alarm.SetInParent()
        return self.request(request).spindle_alarm_state

    def set_main_button_pressed(self, pressed: bool) -> None:
        request = pb.Request()
        request.set_main_button_pressed.pressed = pressed
        self.request(request)

    def set_e_stop_pressed(self, pressed: bool) -> None:
        request = pb.Request()
        request.set_e_stop_pressed.pressed = pressed
        self.request(request)

    def get_front_panel_state(self) -> Any:
        request = pb.Request()
        request.get_front_panel_state.SetInParent()
        return self.request(request).front_panel_state

    def set_temperature(self, sensor: str, celsius: float) -> None:
        request = pb.Request()
        request.set_temperature.sensor = TEMPERATURE_SENSOR_TO_PROTO[sensor]
        request.set_temperature.celsius = celsius
        self.request(request)

    def get_switch_state(self, name: str) -> Any:
        request = pb.Request()
        request.get_switch_state.name = SWITCH_NAME_TO_PROTO[name]
        return self.request(request).switch_state

    def get_laser_state(self) -> Any:
        request = pb.Request()
        request.get_laser_state.SetInParent()
        return self.request(request).laser_state

    def get_pwm_output(self, port: int, pin: int) -> Any:
        request = pb.Request()
        request.get_pwm_output.pin.port = port
        request.get_pwm_output.pin.pin = pin
        return self.request(request).pwm_output

    def get_eeprom_bytes(self, offset: int, length: int) -> Any:
        request = pb.Request()
        request.get_eeprom_bytes.offset = int(offset)
        request.get_eeprom_bytes.length = int(length)
        return self.request(request).eeprom_bytes

    def set_eeprom_bytes(self, offset: int, data: bytes) -> None:
        request = pb.Request()
        request.set_eeprom_bytes.offset = int(offset)
        request.set_eeprom_bytes.data = bytes(data)
        self.request(request)

    def get_eeprom_fields(self) -> list[EepromField]:
        request = pb.Request()
        request.get_eeprom_fields.SetInParent()
        return eeprom_fields_to_state(self.request(request).eeprom_fields)

    def set_eeprom_fields(self, fields: list[EepromField]) -> None:
        request = pb.Request()
        for field in fields:
            output = request.set_eeprom_fields.fields.add()
            output.name = field.name
            if field.type == EepromFieldType.BOOL:
                output.type = pb.EEPROM_FIELD_TYPE_BOOL
                output.boolean = bool(field.value)
            elif field.type == EepromFieldType.INT:
                output.type = pb.EEPROM_FIELD_TYPE_INT
                output.integer = int(field.value)
            else:
                output.type = pb.EEPROM_FIELD_TYPE_FLOAT
                output.number = float(field.value)
        self.request(request)

    def start_interactive_transport(
        self,
        *,
        enable_uart: bool = True,
        tcp_ports: list[int] | None = None,
        log_traffic: bool = False,
    ) -> InteractiveTransportState:
        request = pb.Request()
        request.start_interactive_transport.enable_uart = enable_uart
        request.start_interactive_transport.log_traffic = log_traffic
        for port in tcp_ports if tcp_ports is not None else [0]:
            request.start_interactive_transport.tcp_ports.append(port)
        return interactive_transport_to_state(self.request(request).interactive_transport)

    def stop_interactive_transport(self) -> None:
        request = pb.Request()
        request.stop_interactive_transport.SetInParent()
        self.request(request)

    def machine_snapshot(self) -> pb.MachineSnapshot:
        request = pb.Request()
        request.get_machine_snapshot.SetInParent()
        return self.request(request).machine_snapshot

    def set_atc_pocket_tools(self, tools: list[ToolConfig], *, replace: bool = True) -> None:
        request = pb.Request()
        request.set_atc_pocket_tools.replace = replace
        for tool in tools:
            pocket = request.set_atc_pocket_tools.tools.add()
            pocket.pocket = tool.pocket
            pocket.tool = tool.tool
            pocket.occupied = tool.occupied
            pocket.length_mm = tool.length_mm
            pocket.kind = tool_kind_to_proto(tool.kind)
            pocket.probe_tip_diameter_mm = tool.probe_tip_diameter_mm
        self.request(request)

    def set_spindle_tool(
        self,
        tool: int,
        length_mm: float,
        *,
        installed: bool = True,
        kind: ToolKind | str = ToolKind.UNSPECIFIED,
        probe_tip_diameter_mm: float = 0.0,
    ) -> None:
        request = pb.Request()
        request.set_spindle_tool.installed = installed
        request.set_spindle_tool.tool = int(tool)
        request.set_spindle_tool.length_mm = float(length_mm)
        request.set_spindle_tool.kind = tool_kind_to_proto(kind)
        request.set_spindle_tool.probe_tip_diameter_mm = float(probe_tip_diameter_mm)
        self.request(request)

    @staticmethod
    def _fill_box(target: Any, box: BoxTuple) -> None:
        target.min_x, target.min_y, target.min_z, target.max_x, target.max_y, target.max_z = box
