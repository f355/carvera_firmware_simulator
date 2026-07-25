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

from nicegui import ui

from gui.app_view import AppView
from gui.core.session import SimulatorSession
from gui.presenters.base import SessionPresenter
from gui.presenters.physical import PhysicalPresenter
from gui.presenters.service import ServicePresenter
from gui.presenters.state import StatePresenter
from gui.presenters.tooling import ToolingPresenter
from gui.views.tool_table import collect_tool_table
from gui.views.ui_helpers import event_bool, set_control_locked


class PowerPresenter(SessionPresenter):
    def __init__(
        self,
        session: SimulatorSession,
        *,
        state: StatePresenter,
        physical: PhysicalPresenter,
        tooling: ToolingPresenter,
        service: ServicePresenter,
    ) -> None:
        super().__init__(session)
        self.state = state
        self.physical = physical
        self.tooling = tooling
        self.service = service

    async def power_on(self, view: AppView) -> None:
        assert view.power_switch is not None
        assert view.model_select is not None
        assert view.rotary_accessory_switch is not None
        gui_state = self.session.state_store.snapshot()
        if gui_state.power_transition:
            view.power_switch.value = gui_state.machine_online
            return
        self.session.state_store.set_power_transition(True, machine_model=str(view.model_select.value))
        view.power_switch.disable()
        set_control_locked(view.model_select, True)
        try:
            result = await self.session.process_controller.power_on(
                machine_model=str(view.model_select.value),
                tools=collect_tool_table(view.tool_rows, rack_only=True),
                rotary_enabled=event_bool(view.rotary_accessory_switch),
            )
            await self.state.publish_transport(view, result.transport)
            await self.physical.realtime_speed_changed(view)
            if result.snapshot is not None:
                await self.state.publish_snapshot(view, result.snapshot)
            await self.tooling.apply_physical_boxes(view, notify=False)
            await self.service.refresh_eeprom(view, notify=False)
        except Exception as exc:
            self.session.state_store.mark_offline()
            view.power_switch.value = False
            ui.notify(str(exc), type="negative")
        finally:
            self.state.update_model_selector_lock(view)
            view.power_switch.enable()

    async def power_off(self, view: AppView) -> None:
        assert view.power_switch is not None
        assert view.firmware_state_view is not None
        assert view.model_select is not None
        gui_state = self.session.state_store.snapshot()
        if gui_state.power_transition:
            view.power_switch.value = gui_state.machine_online
            return
        self.session.state_store.set_power_transition(True)
        view.power_switch.disable()
        self.state.update_model_selector_lock(view)
        try:
            await self.session.process_controller.power_off()
            self.session.state_store.mark_offline()
            self.session.backplot_history.clear()
            if view.axis_panel_view is not None:
                view.axis_panel_view.reset()
            if view.transport_panel_view is not None:
                view.transport_panel_view.reset()
            view.firmware_state_view.reset()
            if view.io_panel_view is not None:
                view.io_panel_view.reset(str(view.model_select.value))
            if view.atc_panel_view is not None:
                view.atc_panel_view.reset()
            if view.eeprom_panel_view is not None:
                view.eeprom_panel_view.reset()
            if view.memory_panel_view is not None:
                view.memory_panel_view.reset()
            self.state.update_machine_scene(view, None)
        finally:
            self.state.update_model_selector_lock(view)
            view.power_switch.enable()

    async def power_changed(self, view: AppView, event: Any) -> None:
        if event_bool(event):
            await self.power_on(view)
        else:
            await self.power_off(view)
