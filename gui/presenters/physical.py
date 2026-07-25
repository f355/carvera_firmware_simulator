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
from typing import Any

from nicegui import ui

from gui.app_view import AppView
from gui.core.logging import log_gui_event
from gui.core.session import SimulatorSession
from gui.presenters.base import SessionPresenter
from gui.presenters.state import StatePresenter
from gui.views.environment_tab import GUI_REALTIME_SPEED_MAX
from gui.views.ui_helpers import event_bool, set_status_badge


class PhysicalPresenter(SessionPresenter):
    def __init__(self, session: SimulatorSession, state: StatePresenter) -> None:
        super().__init__(session)
        self.state = state

    def _controls_ready(self, view: AppView) -> bool:
        return view.io_panel_view is not None and self.machine_online

    async def press_main_button(self, view: AppView) -> None:
        if not self.machine_online:
            ui.notify("Power the simulator before pressing the main button.", type="warning")
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_main_button_pressed, True)
            await asyncio.sleep(0.08)
            await self.session.process_controller.call(self.session.client.set_main_button_pressed, False)

    async def front_panel_changed(self, view: AppView, name: str, event: Any) -> None:
        if not self._controls_ready(view):
            return
        with self.notify_client_errors():
            if name == "main_button":
                await self.session.process_controller.call(
                    self.session.client.set_main_button_pressed, event_bool(event)
                )
            else:
                await self.session.process_controller.call(self.session.client.set_e_stop_pressed, event_bool(event))

    async def cover_changed(self, view: AppView, event: Any) -> None:
        # Claim first, and unconditionally: an echo left unclaimed would be
        # mistaken for the user's next real toggle.
        if view.io_panel_view is not None and view.io_panel_view.consume_echoed_write("cover", event_bool(event)):
            return
        if not self._controls_ready(view):
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_cover_open, event_bool(event))

    async def motor_alarm_changed(self, view: AppView, axis: str, event: Any) -> None:
        if not self._controls_ready(view):
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_motor_alarm, axis, event_bool(event))

    async def spindle_alarm_changed(self, view: AppView, event: Any) -> None:
        if not self._controls_ready(view):
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_spindle_alarm, event_bool(event))

    async def rotary_accessory_changed(self, view: AppView, event: Any) -> None:
        installed = event_bool(event)
        if view.machine_tab_view is not None and view.machine_tab_view.rotary_accessory_badge is not None:
            set_status_badge(view.machine_tab_view.rotary_accessory_badge, installed, "connected", "not connected")
        self.state.update_machine_shell_model(view)
        if not self.machine_online:
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_rotary_accessory_installed, installed)

    async def set_temperature(self, view: AppView) -> None:
        if not self.machine_online:
            ui.notify("Power the simulator before setting temperature.", type="warning")
            return
        if view.temperature_sensor is None or view.temperature_celsius is None:
            return
        with self.notify_client_errors():
            sensor = str(view.temperature_sensor.value or "spindle")
            celsius = float(view.temperature_celsius.value or 0.0)
            await self.session.process_controller.call(self.session.client.set_temperature, sensor, celsius)
            if view.io_panel_view is not None:
                view.io_panel_view.set_temperature_status(celsius)

    async def realtime_speed_changed(self, view: AppView, value: float | int | str | None = None) -> None:
        if not self.machine_online:
            ui.notify("Power the simulator before changing realtime speed.", type="warning")
            return
        if view.realtime_speed is None:
            return
        with self.notify_client_errors(TypeError, ValueError):
            source_value = value if value is not None else view.realtime_speed.value
            multiplier = max(0.25, min(GUI_REALTIME_SPEED_MAX, float(source_value or 1.0)))
            view.realtime_speed.value = multiplier
            log_gui_event(f"realtime speed requested={multiplier:g}x")
            await self.session.process_controller.call(self.session.client.set_realtime_speed, multiplier)
