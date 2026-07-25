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

from dataclasses import dataclass, field
from typing import Any

from nicegui import ui

from gui.core.transport_log import TransportLogEntry


@dataclass
class CommsLogTabView:
    container: Any
    autoscroll: Any
    # Mirrors the TransportLogStore cap so the DOM stays bounded too.
    max_rows: int = 2000
    rows: list[Any] = field(default_factory=list)

    def append(self, entry: TransportLogEntry) -> None:
        with self.container:
            with ui.element("div").classes(f"comm-row comm-{entry.direction}") as row:
                ui.label(entry.timestamp or "").classes("comm-time")
                ui.label(entry.channel.upper()).classes("comm-channel")
                ui.label(entry.direction.upper()).classes("comm-direction")
                ui.label(entry.payload).classes("comm-payload")
        self.rows.append(row)
        while len(self.rows) > self.max_rows:
            self.rows.pop(0).delete()
        if bool(self.autoscroll.value):
            self.container.run_method("scrollTo", 0, 1_000_000)

    def clear(self) -> None:
        self.container.clear()
        self.rows.clear()


def build_comms_log_tab() -> CommsLogTabView:
    view: CommsLogTabView
    with ui.element("div").classes("panel-section comm-toolbar"):
        autoscroll = ui.switch("Auto-scroll", value=True).props("dense")
        clear_button = ui.button("Clear").props("dense outline")
    log_container = ui.element("div").classes("comm-log")
    view = CommsLogTabView(container=log_container, autoscroll=autoscroll)
    clear_button.on("click", lambda _: view.clear())
    return view
