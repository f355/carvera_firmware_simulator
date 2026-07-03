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
from gui.protocol.sim_client import SimulatorClientError
from gui.views.environment_tab import GUI_REALTIME_SPEED_MAX
from gui.views.ui_helpers import event_bool, set_status_badge


class PhysicalPresenter(SessionPresenter):
    def __init__(self, session: SimulatorSession, state: StatePresenter) -> None:
        super().__init__(session)
        self.state = state

    async def press_main_button(self, view: AppView) -> None:
        if not self.session.state_store.snapshot().machine_online:
            ui.notify("Power the simulator before pressing the main button.", type="warning")
            return
        try:
            await self.session.process_controller.call(self.session.client.set_main_button_pressed, True)
            await asyncio.sleep(0.08)
            await self.session.process_controller.call(self.session.client.set_main_button_pressed, False)
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def front_panel_changed(self, view: AppView, name: str, event: Any) -> None:
        if (
            view.io_panel_view is None
            or view.io_panel_view.updating_controls
            or not self.session.state_store.snapshot().machine_online
        ):
            return
        try:
            if name == "main_button":
                await self.session.process_controller.call(
                    self.session.client.set_main_button_pressed, event_bool(event)
                )
            else:
                await self.session.process_controller.call(self.session.client.set_e_stop_pressed, event_bool(event))
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def cover_changed(self, view: AppView, event: Any) -> None:
        if (
            view.io_panel_view is None
            or view.io_panel_view.updating_controls
            or not self.session.state_store.snapshot().machine_online
        ):
            return
        try:
            await self.session.process_controller.call(self.session.client.set_cover_open, event_bool(event))
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def motor_alarm_changed(self, view: AppView, axis: str, event: Any) -> None:
        if (
            view.io_panel_view is None
            or view.io_panel_view.updating_controls
            or not self.session.state_store.snapshot().machine_online
        ):
            return
        try:
            await self.session.process_controller.call(self.session.client.set_motor_alarm, axis, event_bool(event))
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def spindle_alarm_changed(self, view: AppView, event: Any) -> None:
        if (
            view.io_panel_view is None
            or view.io_panel_view.updating_controls
            or not self.session.state_store.snapshot().machine_online
        ):
            return
        try:
            await self.session.process_controller.call(self.session.client.set_spindle_alarm, event_bool(event))
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def rotary_accessory_changed(self, view: AppView, event: Any) -> None:
        installed = event_bool(event)
        if view.machine_tab_view is not None and view.machine_tab_view.rotary_accessory_badge is not None:
            set_status_badge(view.machine_tab_view.rotary_accessory_badge, installed, "connected", "not connected")
        self.state.update_machine_shell_model(view)
        if not self.session.state_store.snapshot().machine_online:
            return
        try:
            await self.session.process_controller.call(self.session.client.set_rotary_accessory_installed, installed)
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def set_temperature(self, view: AppView) -> None:
        if not self.session.state_store.snapshot().machine_online:
            ui.notify("Power the simulator before setting temperature.", type="warning")
            return
        if view.temperature_sensor is None or view.temperature_celsius is None:
            return
        try:
            sensor = str(view.temperature_sensor.value or "spindle")
            celsius = float(view.temperature_celsius.value or 0.0)
            await self.session.process_controller.call(self.session.client.set_temperature, sensor, celsius)
            if view.io_panel_view is not None:
                view.io_panel_view.set_temperature_status(celsius)
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def realtime_speed_changed(self, view: AppView, value: float | int | str | None = None) -> None:
        if not self.session.state_store.snapshot().machine_online:
            ui.notify("Power the simulator before changing realtime speed.", type="warning")
            return
        if view.realtime_speed is None:
            return
        try:
            source_value = value if value is not None else view.realtime_speed.value
            multiplier = max(0.25, min(GUI_REALTIME_SPEED_MAX, float(source_value or 1.0)))
            view.realtime_speed.value = multiplier
            log_gui_event(f"realtime speed requested={multiplier:g}x")
            await self.session.process_controller.call(self.session.client.set_realtime_speed, multiplier)
        except (SimulatorClientError, TypeError, ValueError) as exc:
            ui.notify(str(exc), type="negative")
