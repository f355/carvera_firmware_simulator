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

from gui.protocol import model


def test_machine_snapshot_and_telemetry_use_typed_domain_state() -> None:
    snapshot = model.pb.MachineSnapshot()
    snapshot.firmware_booted = True
    snapshot.homed = True
    snapshot.soft_endstop_enabled = True
    snapshot.work_area.min_x = -200.0
    snapshot.work_area.max_x = 1.0
    snapshot.physical_travel.min_z = -122.0
    snapshot.physical_travel.max_z = 1.0
    snapshot.tool_setter_available = True
    snapshot.tool_setter.min_x = -15.0
    snapshot.tool_setter.max_x = -7.0
    snapshot.memory.main.capacity_bytes = 32_568
    snapshot.memory.main.live_payload_bytes = 5_000
    snapshot.memory.main.peak_live_payload_bytes = 6_000
    snapshot.memory.main.total_free_bytes = 18_000
    snapshot.memory.main.minimum_margin_bytes = 9_000
    snapshot.memory.ahb.capacity_bytes = 32_768
    snapshot.memory.ahb.live_payload_bytes = 12_500
    snapshot.memory.ahb.total_free_bytes = 4_700
    snapshot.memory.unresolved_main_live_host_bytes = 20_000

    axis = snapshot.axes.add()
    axis.axis = model.pb.AXIS_X
    axis.physical_mm = -2.0
    axis.machine_position = -1.0
    axis.endstop_triggered = True

    snapshot.spindle.actual_rpm = 1234.0
    snapshot.spindle.max_rpm = 14_500.0
    snapshot.atc.available = True
    snapshot.atc.spindle.active_tool = 2
    snapshot.atc.spindle.length_mm = 41.3
    pocket = snapshot.atc.pockets.add()
    pocket.pocket = 1
    pocket.tool = 2
    pocket.occupied = True
    pocket.length_mm = 41.3
    pocket.kind = model.pb.TOOL_KIND_CUTTING_TOOL

    state = model.snapshot_to_state(snapshot)

    assert state.firmware_booted is True
    assert state.work_area is not None
    assert state.work_area.min_x == -200.0
    assert state.physical_travel is not None
    assert state.physical_travel.min_z == -122.0
    assert state.tool_setter is not None
    assert state.tool_setter.center_x == -11.0
    assert state.axis("X").physical_mm == -2.0
    assert state.axis("X").endstop_triggered is True
    assert state.spindle is not None
    assert state.spindle.max_rpm == 14_500.0
    assert state.atc is not None
    assert state.atc.available is True
    assert state.atc.spindle.active_tool == 2
    assert state.atc.pocket(1).kind == model.ToolKind.CUTTING_TOOL
    assert state.memory is not None
    assert state.memory.main.capacity_bytes == 32_568
    assert state.memory.main.live_payload_bytes == 5_000
    assert state.memory.main.minimum_margin_bytes == 9_000
    assert state.memory.ahb.live_payload_bytes == 12_500
    assert state.memory.unresolved_main_live_host_bytes == 20_000

    telemetry = model.pb.MachineTelemetry()
    telemetry.firmware_booted = True
    telemetry.time_us = 12_340_000
    telemetry.axes.add(axis=model.pb.AXIS_A, physical_mm=10.5, physical_speed_per_min=72.25)

    frame = model.telemetry_to_state(telemetry)
    assert frame.axis("A").physical_mm == 10.5
    assert frame.axis("A").physical_speed_per_min == 72.25
    assert frame.work_area is None
    assert frame.atc is None
    assert frame.memory is None
    assert frame.telemetry_time_s == 12.34


def test_eeprom_protobuf_uses_structured_domain_state() -> None:
    contents = model.pb.EepromContents(
        tool_length_offset=-31.32,
        reference_machine_z=-122.0,
        tool_machine_z=-70.0,
        reserved=3.5,
        active_tool=2,
        tool_not_calibrated=True,
        current_wcs=1,
    )
    contents.persistent_variables.add(number=501, value=12.5)
    contents.work_coordinate_systems.add(number=54, x=1.0, y=2.0, z=3.0, a=4.0, rotation=5.0)

    converted = model.eeprom_contents_to_state(contents)
    assert converted == model.EepromContents(
        tool_length_offset=-31.32,
        reference_machine_z=-122.0,
        tool_machine_z=-70.0,
        reserved=3.5,
        active_tool=2,
        tool_not_calibrated=True,
        current_wcs=1,
        persistent_variables=(model.PersistentVariable(number=501, value=12.5),),
        work_coordinate_systems=(model.WorkCoordinateSystem(number=54, x=1.0, y=2.0, z=3.0, a=4.0, rotation=5.0),),
    )
    assert model.eeprom_contents_to_proto(converted) == contents


def test_transport_protobuf_uses_typed_domain_state() -> None:

    transport = model.pb.InteractiveTransport()
    transport.uart_supported = True
    transport.uart_path = "/dev/ttys123"
    endpoint = transport.tcp_endpoints.add()
    endpoint.host = "127.0.0.1"
    endpoint.port = 2222

    assert model.interactive_transport_to_state(transport) == model.InteractiveTransportState(
        uart_supported=True,
        uart_path="/dev/ttys123",
        tcp_endpoints=(model.TransportEndpoint(host="127.0.0.1", port=2222),),
    )
