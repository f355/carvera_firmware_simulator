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

from nicegui import ui

from gui.app_view import AppView
from gui.presenters.base import SessionPresenter


class ServicePresenter(SessionPresenter):
    async def refresh_memory_details(self, view: AppView) -> None:
        if view.memory_panel_view is None:
            return
        if not self.machine_online:
            ui.notify("Power on the simulator before reading memory details.", type="warning")
            return
        with self.notify_client_errors():
            details = await self.session.process_controller.call(self.session.client.get_memory_details)
            view.memory_panel_view.set_details(details)

    async def refresh_eeprom(self, view: AppView, *, notify: bool = True) -> None:
        if view.eeprom_panel_view is None:
            return
        if not self.machine_online:
            view.eeprom_panel_view.reset()
            if notify:
                ui.notify("Power on the simulator before reading EEPROM.", type="warning")
            return
        with self.notify_client_errors():
            contents = await self.session.process_controller.call(self.session.client.get_eeprom_contents)
            view.eeprom_panel_view.set_contents(contents)
            if notify:
                ui.notify("EEPROM contents refreshed", type="positive")

    async def write_eeprom(self, view: AppView) -> None:
        if view.eeprom_panel_view is None:
            return
        if not self.machine_online:
            ui.notify("Power on the simulator before writing EEPROM.", type="warning")
            return
        with self.notify_client_errors(TypeError, ValueError):
            await self.session.process_controller.call(
                self.session.client.set_eeprom_contents, view.eeprom_panel_view.edited_contents()
            )
            await self.refresh_eeprom(view, notify=False)
            ui.notify("EEPROM contents written; reset firmware to reload running modules.", type="positive")
