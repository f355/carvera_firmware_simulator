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

from typing import Any

from gui.app_view import AppView
from gui.presenters.base import SessionPresenter
from gui.protocol.model import InteractiveTransportState, MachineState, pb, snapshot_to_state
from gui.views.ui_helpers import event_bool, set_control_locked, set_status_badge


class StatePresenter(SessionPresenter):
    async def publish_snapshot(self, view: AppView, snapshot: pb.MachineSnapshot) -> None:
        self.publish_machine_state(view, snapshot_to_state(snapshot))

    def publish_machine_state(self, view: AppView, state: MachineState) -> None:
        merged_state = self.session.state_store.apply_full_snapshot(state).machine_state
        assert merged_state is not None
        self._render_machine_state(view, merged_state)

    async def publish_transport(self, view: AppView, transport: InteractiveTransportState | None) -> None:
        model = str(view.model_select.value) if view.model_select is not None else None
        self.session.state_store.set_online(transport=transport, machine_model=model)
        if view.transport_panel_view is not None and transport is not None:
            view.transport_panel_view.update(transport)

    def update_machine_shell_model(self, view: AppView) -> None:
        assert view.model_select is not None
        if view.machine_scene_view is not None:
            view.machine_scene_view.update_shell_model(str(view.model_select.value))

    def cad_models_changed(self, view: AppView, event: Any) -> None:
        if view.machine_scene_view is not None:
            view.machine_scene_view.set_cad_models_visible(event_bool(event))

    def clear_backplot(self, view: AppView) -> None:
        if view.machine_scene_view is not None:
            view.machine_scene_view.clear_backplot()

    def update_model_selector_lock(self, view: AppView) -> None:
        assert view.model_select is not None
        snapshot = self.session.state_store.snapshot()
        set_control_locked(view.model_select, snapshot.machine_online or snapshot.power_transition)

    def update_machine_scene(self, view: AppView, state: MachineState | None) -> None:
        if view.machine_scene_view is not None:
            view.machine_scene_view.update(state)

    def restore_view(self, view: AppView) -> None:
        gui_state = self.session.state_store.snapshot()
        if view.model_select is not None and gui_state.machine_model is not None:
            view.model_select.value = gui_state.machine_model
            self.update_machine_shell_model(view)
        if view.power_switch is not None:
            view.power_switch.value = gui_state.machine_online or gui_state.power_transition
            if gui_state.power_transition:
                view.power_switch.disable()
            else:
                view.power_switch.enable()
        self.update_model_selector_lock(view)
        if view.transport_panel_view is not None and gui_state.transport is not None:
            view.transport_panel_view.update(gui_state.transport)
        if gui_state.machine_state is not None:
            self._render_machine_state(view, gui_state.machine_state)
        if view.machine_scene_view is not None:
            view.machine_scene_view.restore_backplot()

    def _render_machine_state(self, view: AppView, state: MachineState) -> None:
        assert view.firmware_state_view is not None
        set_status_badge(view.firmware_state_view.firmware_badge, state.firmware_booted, "booted", "off")
        set_status_badge(view.firmware_state_view.homed_badge, state.homed, "homed", "not homed")
        set_status_badge(view.firmware_state_view.soft_limit_badge, state.soft_endstop_enabled, "enabled", "disabled")
        if view.axis_panel_view is not None:
            view.axis_panel_view.update(state)
        if view.atc_panel_view is not None:
            view.atc_panel_view.update(state.atc)
        self.update_machine_scene(view, state)

    def drain_telemetry(self, view: AppView) -> None:
        assert view.firmware_state_view is not None
        view.telemetry_cursor, latest = self.session.telemetry_buffer.latest_since(view.telemetry_cursor)
        if latest is None:
            return
        gui_state = self.session.state_store.snapshot()
        if not (gui_state.machine_online or gui_state.power_transition):
            return

        self.session.state_store.apply_telemetry(latest)
        if view.axis_panel_view is not None:
            view.axis_panel_view.update(latest)
        self.update_machine_scene(view, latest)

    def drain_snapshots(self, view: AppView) -> None:
        assert view.firmware_state_view is not None
        gui_state = self.session.state_store.snapshot()
        if not (gui_state.machine_online or gui_state.power_transition):
            return
        view.snapshot_cursor, latest = self.session.snapshot_buffer.latest_since(view.snapshot_cursor)
        if latest is None:
            return
        self.publish_machine_state(view, latest)

    def drain_physical_io(self, view: AppView) -> None:
        if view.io_panel_view is None:
            return
        gui_state = self.session.state_store.snapshot()
        if not (gui_state.machine_online or gui_state.power_transition):
            return
        view.physical_io_cursor, latest = self.session.physical_io_buffer.latest_since(view.physical_io_cursor)
        if latest is None:
            return
        view.io_panel_view.apply_snapshot(latest)

    def drain_transport_log(self, view: AppView) -> None:
        if view.comms_log_view is None:
            return
        view.transport_log_cursor, entries = self.session.transport_log_store.cursor_and_entries_since(
            view.transport_log_cursor
        )
        for entry in entries:
            view.comms_log_view.append(entry)
