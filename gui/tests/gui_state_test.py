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

from gui.core.gui_state import GuiStateStore
from gui.protocol.model import AxisSnapshot, Box3D, InteractiveTransportState, MachineState, SpindleSnapshot
from gui.protocol.model import TransportEndpoint


def test_gui_state_store_publishes_versioned_snapshots() -> None:
    store = GuiStateStore()

    version, snapshot = store.latest_since(0)
    assert version == 0
    assert snapshot is None

    store.set_power_transition(True)
    version, snapshot = store.latest_since(0)
    assert version == 1
    assert snapshot is not None
    assert snapshot.power_transition is True
    assert snapshot.machine_online is False

    transport = InteractiveTransportState(
        uart_supported=True,
        uart_path="/dev/ttys123",
        tcp_endpoints=(TransportEndpoint("127.0.0.1", 2222),),
    )
    machine = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-10.0, -10.0, -10.0, 1.0, 1.0, 1.0),
        physical_travel=None,
        axes=(AxisSnapshot("X", 0, -2.0, -2.0, False),),
        atc=None,
        spindle=None,
        tool_setter=None,
    )

    store.set_online(transport=transport)
    store.set_machine_state(machine)
    version, snapshot = store.latest_since(version)
    assert version == 3
    assert snapshot is not None
    assert snapshot.machine_online is True
    assert snapshot.power_transition is False
    assert snapshot.transport == transport
    assert snapshot.machine_state == machine

    repeated_version, repeated_snapshot = store.latest_since(version)
    assert repeated_version == version
    assert repeated_snapshot is None

    store.mark_offline()
    version, snapshot = store.latest_since(version)
    assert snapshot is not None
    assert snapshot.machine_online is False
    assert snapshot.transport is None
    assert snapshot.machine_state is None


def test_gui_state_store_merges_telemetry_into_latest_full_machine_state() -> None:
    store = GuiStateStore()
    full_state = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-10.0, -10.0, -10.0, 1.0, 1.0, 1.0),
        physical_travel=Box3D(-11.0, -11.0, -11.0, 2.0, 2.0, 2.0),
        axes=(AxisSnapshot("X", 0, -2.0, -2.0, False),),
        atc=None,
        spindle=SpindleSnapshot(False, 0.0, 0.0, 14_500.0),
        tool_setter=Box3D(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0),
    )
    telemetry = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=False,
        work_area=None,
        physical_travel=None,
        axes=(AxisSnapshot("X", 100, -1.5, -1.5, False),),
        atc=None,
        spindle=SpindleSnapshot(True, 1000.0, 10_000.0, 14_500.0),
        tool_setter=None,
    )

    store.set_machine_state(full_state)
    store.apply_telemetry(telemetry)

    snapshot = store.snapshot()
    assert snapshot.machine_state is not None
    assert snapshot.machine_state.work_area == full_state.work_area
    assert snapshot.machine_state.physical_travel == full_state.physical_travel
    assert snapshot.machine_state.tool_setter == full_state.tool_setter
    assert snapshot.machine_state.axes == telemetry.axes
    assert snapshot.machine_state.spindle == telemetry.spindle


def test_gui_state_store_preserves_live_axes_when_full_snapshot_lags() -> None:
    store = GuiStateStore()
    initial_snapshot = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-10.0, -10.0, -10.0, 1.0, 1.0, 1.0),
        physical_travel=Box3D(-11.0, -11.0, -11.0, 2.0, 2.0, 2.0),
        axes=(AxisSnapshot("X", 100, -1.0, -1.0, False),),
        atc=None,
        spindle=None,
        tool_setter=Box3D(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0),
    )
    live_telemetry = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=False,
        work_area=None,
        physical_travel=None,
        axes=(AxisSnapshot("X", 130, -0.7, -0.7, False),),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    lagging_snapshot = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-20.0, -10.0, -10.0, 1.0, 1.0, 1.0),
        physical_travel=Box3D(-21.0, -11.0, -11.0, 2.0, 2.0, 2.0),
        axes=(AxisSnapshot("X", 110, -0.9, -0.9, False),),
        atc=None,
        spindle=None,
        tool_setter=Box3D(-2.0, -1.0, -1.0, 1.0, 1.0, 1.0),
    )

    store.apply_full_snapshot(initial_snapshot)
    store.apply_telemetry(live_telemetry)
    store.apply_full_snapshot(lagging_snapshot)

    snapshot = store.snapshot()
    assert snapshot.machine_state is not None
    assert snapshot.machine_state.axes == live_telemetry.axes
    assert snapshot.machine_state.work_area == lagging_snapshot.work_area
    assert snapshot.machine_state.tool_setter == lagging_snapshot.tool_setter
