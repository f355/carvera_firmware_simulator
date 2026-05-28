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

from types import SimpleNamespace
from typing import Any, cast

from gui.app_actions import AppActions
from gui.app_view import AppView
from gui.core.gui_state import GuiStateStore
from gui.protocol.model import AxisSnapshot, Box3D, MachineState
from gui.views.machine_tab import FirmwareStateView


class FakeStateBuffer:
    def __init__(self, state: MachineState) -> None:
        self.state = state
        self.last_cursor: int | None = None

    def latest_since(self, _cursor: int) -> tuple[int, MachineState]:
        self.last_cursor = _cursor
        return 1, self.state


class FakeAxisPanel:
    def __init__(self) -> None:
        self.updated_with: MachineState | None = None

    def update(self, state: MachineState) -> None:
        self.updated_with = state


class FakeMachineScene:
    def __init__(self) -> None:
        self.updated_with: MachineState | None = None

    def update(self, state: MachineState | None) -> None:
        self.updated_with = state


class FakeLabel:
    def __init__(self) -> None:
        self.text = ""
        self.classes_text = ""

    def classes(self, *, add: str | None = None, remove: str | None = None) -> None:
        if remove is not None:
            removed = set(remove.split())
            self.classes_text = " ".join(part for part in self.classes_text.split() if part not in removed)
        if add is not None:
            existing = self.classes_text.split()
            for part in add.split():
                if part not in existing:
                    existing.append(part)
            self.classes_text = " ".join(existing)


def make_firmware_state_view() -> FirmwareStateView:
    return FirmwareStateView(
        firmware_badge=FakeLabel(),
        homed_badge=FakeLabel(),
        soft_limit_badge=FakeLabel(),
    )


def test_drain_telemetry_updates_scene_while_powering_on() -> None:
    store = GuiStateStore()
    store.set_power_transition(True)
    telemetry = MachineState(
        firmware_booted=True,
        homed=False,
        soft_endstop_enabled=False,
        work_area=None,
        physical_travel=Box3D(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0),
        axes=(AxisSnapshot("X", 100, -10.0, -10.0, False),),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    session = SimpleNamespace(state_store=store, telemetry_buffer=FakeStateBuffer(telemetry))
    actions = AppActions(session)  # type: ignore[arg-type]
    axis_panel = FakeAxisPanel()
    machine_scene = FakeMachineScene()
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())
    view.axis_panel_view = cast(Any, axis_panel)
    view.machine_scene_view = cast(Any, machine_scene)

    actions.drain_telemetry(view)

    assert store.snapshot().machine_state == telemetry
    assert axis_panel.updated_with == telemetry
    assert machine_scene.updated_with == telemetry


def test_drain_snapshot_updates_full_firmware_state_and_scene() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    snapshot = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-10.0, -20.0, -30.0, 1.0, 2.0, 3.0),
        physical_travel=Box3D(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0),
        axes=(),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    session = SimpleNamespace(state_store=store, snapshot_buffer=FakeStateBuffer(snapshot))
    actions = AppActions(session)  # type: ignore[arg-type]
    firmware_view = make_firmware_state_view()
    machine_scene = FakeMachineScene()
    view = AppView()
    view.firmware_state_view = cast(Any, firmware_view)
    view.machine_scene_view = cast(Any, machine_scene)

    actions.drain_snapshots(view)

    assert store.snapshot().machine_state == snapshot
    assert firmware_view.firmware_badge.text == "booted"
    assert firmware_view.firmware_badge.classes_text == "badge-on"
    assert firmware_view.homed_badge.text == "homed"
    assert firmware_view.homed_badge.classes_text == "badge-on"
    assert firmware_view.soft_limit_badge.text == "enabled"
    assert firmware_view.soft_limit_badge.classes_text == "badge-on"
    assert machine_scene.updated_with == snapshot


def test_drain_snapshot_updates_during_power_transition() -> None:
    store = GuiStateStore()
    store.set_power_transition(True)
    snapshot = MachineState(
        firmware_booted=True,
        homed=False,
        soft_endstop_enabled=False,
        work_area=Box3D(-10.0, -20.0, -30.0, 1.0, 2.0, 3.0),
        physical_travel=Box3D(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0),
        axes=(),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    session = SimpleNamespace(state_store=store, snapshot_buffer=FakeStateBuffer(snapshot))
    actions = AppActions(session)  # type: ignore[arg-type]
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())
    view.machine_scene_view = cast(Any, FakeMachineScene())

    actions.drain_snapshots(view)

    assert store.snapshot().machine_state == snapshot


def test_drain_snapshot_does_not_advance_cursor_while_offline() -> None:
    store = GuiStateStore()
    snapshot = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=None,
        physical_travel=None,
        axes=(),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    buffer = FakeStateBuffer(snapshot)
    session = SimpleNamespace(state_store=store, snapshot_buffer=buffer)
    actions = AppActions(session)  # type: ignore[arg-type]
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())

    actions.drain_snapshots(view)

    assert buffer.last_cursor is None
    assert view.snapshot_cursor == 0
