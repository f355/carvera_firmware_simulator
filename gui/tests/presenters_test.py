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

import asyncio
from types import SimpleNamespace
from typing import Any, cast

from gui.app_view import AppView
from gui.core.gui_state import GuiStateStore
from gui.presenters.app import AppPresenters
from gui.presenters.appearance import AppearancePresenter
from gui.presenters.physical import PhysicalPresenter
from gui.presenters.service import ServicePresenter
from gui.presenters.state import StatePresenter
from gui.presenters.tooling import ToolingPresenter
from gui.protocol.model import AxisSnapshot, Box3D, InteractiveTransportState, MachineState, TransportEndpoint
from gui.tests.fakes import FakeControl, FakeLabel
from gui.views.environment_tab import GUI_REALTIME_SPEED_MAX
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
        self.shell_model: str | None = None
        self.backplot_restored = False
        self.stock_box: tuple[Box3D, bool] | None = None

    def update(self, state: MachineState | None) -> None:
        self.updated_with = state

    def update_shell_model(self, model: str) -> None:
        self.shell_model = model

    def restore_backplot(self) -> None:
        self.backplot_restored = True

    def set_stock_box(self, box: Box3D, *, enabled: bool) -> None:
        self.stock_box = (box, enabled)


class FakeTransportPanel:
    def __init__(self) -> None:
        self.updated_with: InteractiveTransportState | None = None

    def update(self, value: InteractiveTransportState) -> None:
        self.updated_with = value


class FakeMemoryPanel:
    def __init__(self) -> None:
        self.summary: object | None = None
        self.details: object | None = None

    def update_summary(self, value: object) -> None:
        self.summary = value

    def set_details(self, value: object) -> None:
        self.details = value


class FakeProcessController:
    def __init__(self, result: object = None) -> None:
        self.calls: list[tuple[object, tuple[object, ...]]] = []
        self.keyword_calls: list[dict[str, object]] = []
        self.result = result

    async def call(self, method: object, *args: object, **kwargs: object) -> object:
        self.calls.append((method, args))
        self.keyword_calls.append(kwargs)
        return self.result


class FakeClient:
    def set_realtime_speed(self, multiplier: float) -> None:
        _ = multiplier

    def get_memory_details(self) -> None:
        return None

    def set_stock_box(self, box: tuple[float, float, float, float, float, float], *, enabled: bool = True) -> None:
        _ = box, enabled


def test_app_presenters_is_composed_from_features() -> None:
    actions = AppPresenters(SimpleNamespace())  # type: ignore[arg-type]
    assert isinstance(actions.state, StatePresenter)
    assert isinstance(actions.tooling, ToolingPresenter)
    assert isinstance(actions.physical, PhysicalPresenter)
    assert isinstance(actions.service, ServicePresenter)
    assert isinstance(actions.appearance, AppearancePresenter)


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
    actions = AppPresenters(session)  # type: ignore[arg-type]
    axis_panel = FakeAxisPanel()
    machine_scene = FakeMachineScene()
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())
    view.axis_panel_view = cast(Any, axis_panel)
    view.machine_scene_view = cast(Any, machine_scene)

    actions.state.drain_telemetry(view)

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
    actions = AppPresenters(session)  # type: ignore[arg-type]
    firmware_view = make_firmware_state_view()
    machine_scene = FakeMachineScene()
    view = AppView()
    view.firmware_state_view = cast(Any, firmware_view)
    view.machine_scene_view = cast(Any, machine_scene)

    actions.state.drain_snapshots(view)

    assert store.snapshot().machine_state == snapshot
    assert firmware_view.firmware_badge.text == "booted"
    assert firmware_view.firmware_badge.classes_text == "badge-on"
    assert firmware_view.homed_badge.text == "homed"
    assert firmware_view.homed_badge.classes_text == "badge-on"
    assert firmware_view.soft_limit_badge.text == "enabled"
    assert firmware_view.soft_limit_badge.classes_text == "badge-on"
    assert machine_scene.updated_with == snapshot


def test_periodic_snapshot_updates_memory_panel() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    summary = object()
    snapshot = MachineState(
        firmware_booted=True,
        homed=False,
        soft_endstop_enabled=False,
        work_area=None,
        physical_travel=None,
        axes=(),
        atc=None,
        spindle=None,
        tool_setter=None,
        memory=cast(Any, summary),
    )
    session = SimpleNamespace(state_store=store, snapshot_buffer=FakeStateBuffer(snapshot))
    actions = AppPresenters(session)  # type: ignore[arg-type]
    memory_panel = FakeMemoryPanel()
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())
    view.memory_panel_view = cast(Any, memory_panel)

    actions.state.drain_snapshots(view)

    assert memory_panel.summary is summary


def test_memory_allocation_details_are_requested_on_demand() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    details = object()
    process_controller = FakeProcessController(details)
    client = FakeClient()
    session = SimpleNamespace(state_store=store, process_controller=process_controller, client=client)
    actions = AppPresenters(session)  # type: ignore[arg-type]
    memory_panel = FakeMemoryPanel()
    view = AppView()
    view.memory_panel_view = cast(Any, memory_panel)

    asyncio.run(actions.service.refresh_memory_details(view))

    assert process_controller.calls == [(client.get_memory_details, ())]
    assert memory_panel.details is details


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
    actions = AppPresenters(session)  # type: ignore[arg-type]
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())
    view.machine_scene_view = cast(Any, FakeMachineScene())

    actions.state.drain_snapshots(view)

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
    actions = AppPresenters(session)  # type: ignore[arg-type]
    view = AppView()
    view.firmware_state_view = cast(Any, make_firmware_state_view())

    actions.state.drain_snapshots(view)

    assert buffer.last_cursor is None
    assert view.snapshot_cursor == 0


def test_restore_view_applies_running_machine_state_to_new_page() -> None:
    store = GuiStateStore()
    transport = InteractiveTransportState(
        uart_supported=True,
        uart_path="/dev/ttys123",
        tcp_endpoints=(TransportEndpoint("127.0.0.1", 2222),),
    )
    state = MachineState(
        firmware_booted=True,
        homed=True,
        soft_endstop_enabled=True,
        work_area=Box3D(-10.0, -10.0, -10.0, 1.0, 1.0, 1.0),
        physical_travel=Box3D(-11.0, -11.0, -11.0, 2.0, 2.0, 2.0),
        axes=(AxisSnapshot("X", 10, -2.0, -2.0, False),),
        atc=None,
        spindle=None,
        tool_setter=None,
    )
    store.set_power_transition(True, machine_model="ca1")
    store.set_online(transport=transport, machine_model="ca1")
    store.set_machine_state(state)
    session = SimpleNamespace(state_store=store)
    actions = AppPresenters(session)  # type: ignore[arg-type]
    firmware_view = make_firmware_state_view()
    axis_panel = FakeAxisPanel()
    machine_scene = FakeMachineScene()
    transport_panel = FakeTransportPanel()
    view = AppView()
    model_select = FakeControl("c1")
    power_switch = FakeControl(False)
    view.header.model_select = model_select
    view.header.power_switch = power_switch
    view.firmware_state_view = cast(Any, firmware_view)
    view.axis_panel_view = cast(Any, axis_panel)
    view.machine_scene_view = cast(Any, machine_scene)
    view.transport_panel_view = cast(Any, transport_panel)

    actions.state.restore_view(view)

    assert model_select.value == "ca1"
    assert model_select.disabled is True
    assert power_switch.value is True
    assert power_switch.disabled is False
    assert firmware_view.firmware_badge.text == "booted"
    assert firmware_view.homed_badge.text == "homed"
    assert axis_panel.updated_with == state
    assert machine_scene.updated_with == state
    assert machine_scene.shell_model == "ca1"
    assert machine_scene.backplot_restored is True
    assert transport_panel.updated_with == transport


def test_realtime_speed_changed_sends_selected_multiplier() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    process_controller = FakeProcessController()
    client = FakeClient()
    session = SimpleNamespace(
        state_store=store,
        process_controller=process_controller,
        client=client,
    )
    actions = AppPresenters(session)  # type: ignore[arg-type]
    view = AppView()
    view.environment_tab_view = SimpleNamespace(realtime_speed=FakeControl(5.0))  # type: ignore[assignment]

    asyncio.run(actions.physical.realtime_speed_changed(view))

    assert process_controller.calls == [(client.set_realtime_speed, (5.0,))]


def test_applying_stock_updates_collision_geometry_and_scene_together() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    process_controller = FakeProcessController()
    client = FakeClient()
    session = SimpleNamespace(state_store=store, process_controller=process_controller, client=client)
    actions = AppPresenters(session)  # type: ignore[arg-type]
    values = (-288.669, -201.902, -122.0, -138.669, -51.902, -112.0)
    names = ("min_x", "min_y", "min_z", "max_x", "max_y", "max_z")
    controls = {"enabled": FakeControl(True)}
    controls.update({name: FakeControl(value) for name, value in zip(names, values, strict=True)})
    scene = FakeMachineScene()
    view = AppView()
    view.stock_tab_view = SimpleNamespace(box_controls={"stock": controls})  # type: ignore[assignment]
    view.machine_scene_view = cast(Any, scene)

    asyncio.run(actions.tooling.apply_physical_boxes(view, notify=False))

    assert process_controller.calls == [(client.set_stock_box, (values,))]
    assert process_controller.keyword_calls == [{"enabled": True}]
    assert scene.stock_box == (Box3D(*values), True)


def test_realtime_speed_changed_uses_event_value_before_control_catches_up() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    process_controller = FakeProcessController()
    client = FakeClient()
    session = SimpleNamespace(state_store=store, process_controller=process_controller, client=client)
    actions = AppPresenters(session)  # type: ignore[arg-type]
    view = AppView()
    view.environment_tab_view = SimpleNamespace(realtime_speed=FakeControl(1.0))  # type: ignore[assignment]

    asyncio.run(actions.physical.realtime_speed_changed(view, value=6.0))

    assert process_controller.calls == [(client.set_realtime_speed, (6.0,))]


def test_realtime_speed_changed_clamps_to_gui_cap() -> None:
    store = GuiStateStore()
    store.set_online(transport=None)
    process_controller = FakeProcessController()
    client = FakeClient()
    session = SimpleNamespace(state_store=store, process_controller=process_controller, client=client)
    actions = AppPresenters(session)  # type: ignore[arg-type]
    speed_control = FakeControl(1.0)
    view = AppView()
    view.environment_tab_view = SimpleNamespace(realtime_speed=speed_control)  # type: ignore[assignment]

    asyncio.run(actions.physical.realtime_speed_changed(view, value=19.5))

    assert process_controller.calls == [(client.set_realtime_speed, (GUI_REALTIME_SPEED_MAX,))]
    assert speed_control.value == GUI_REALTIME_SPEED_MAX
