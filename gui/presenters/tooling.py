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
from gui.protocol.model import Box3D, ToolKind
from gui.views.stock_tab import load_default_stock as load_default_stock_controls
from gui.views.tool_table import (
    clear_tool_occupancy,
    collect_box_values,
    collect_tool_table,
    inferred_tool_kind,
    physical_length_from_stickout,
)
from gui.views.tool_table import (
    load_default_tools as load_default_tool_controls,
)
from gui.views.ui_helpers import event_bool


class ToolingPresenter(SessionPresenter):
    async def apply_atc_table(self, view: AppView, *, notify: bool = True) -> None:
        tools = collect_tool_table(view.tool_rows, rack_only=True)
        if not self.machine_online:
            if notify:
                ui.notify("Tool table will be applied when the simulator powers on.", type="info")
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_atc_pocket_tools, tools)
            if notify:
                ui.notify("ATC rack updated", type="positive")

    def load_default_tools(self, view: AppView) -> None:
        load_default_tool_controls(view.tool_rows)

    def clear_tool_table(self, view: AppView) -> None:
        clear_tool_occupancy(view.tool_rows)

    def load_default_stock(self, view: AppView) -> None:
        if view.stock_tab_view is not None and view.model_select is not None:
            load_default_stock_controls(view.stock_tab_view, str(view.model_select.value))

    def collect_box(self, view: AppView, name: str) -> tuple[float, float, float, float, float, float]:
        return collect_box_values(view.box_controls[name])

    async def apply_physical_boxes(self, view: AppView, *, notify: bool = True) -> None:
        box_values = self.collect_box(view, "stock")
        enabled = event_bool(view.box_controls["stock"]["enabled"])
        if not self.machine_online:
            if view.machine_scene_view is not None:
                view.machine_scene_view.set_stock_box(Box3D(*box_values), enabled=enabled)
            if notify:
                ui.notify("Stock geometry will be applied when the simulator powers on.", type="info")
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(
                self.session.client.set_stock_box,
                box_values,
                enabled=enabled,
            )
            if view.machine_scene_view is not None:
                view.machine_scene_view.set_stock_box(Box3D(*box_values), enabled=enabled)
            if notify:
                ui.notify("Stock geometry updated", type="positive")

    async def load_spindle_tool(
        self,
        view: AppView,
        tool: int,
        length_mm: float,
        *,
        kind: ToolKind | str = ToolKind.UNSPECIFIED,
        probe_tip_diameter_mm: float = 0.0,
    ) -> None:
        if not self.machine_online:
            ui.notify("Power on the simulator before loading a spindle tool.", type="warning")
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(
                self.session.client.set_spindle_tool,
                tool,
                length_mm,
                installed=True,
                kind=kind,
                probe_tip_diameter_mm=probe_tip_diameter_mm,
            )
            ui.notify(f"Tool {tool} loaded in spindle", type="positive")

    async def load_spindle_tool_from_pocket(self, view: AppView, pocket: int) -> None:
        row = view.tool_rows.get(pocket)
        if row is None:
            return
        if not bool(row["occupied"].value):
            ui.notify(f"Pocket {pocket} is empty.", type="warning")
            return
        await self.apply_atc_table(view, notify=False)
        tool_number = int(row.get("tool_number", getattr(row["tool"], "value", 0)) or 0)
        await self.load_spindle_tool(
            view,
            tool_number,
            physical_length_from_stickout(float(row["stickout"].value or 0.0)),
            kind=inferred_tool_kind(tool_number),
            probe_tip_diameter_mm=float(row["probe_tip"].value or 0.0),
        )

    async def unload_spindle_tool(self, view: AppView) -> None:
        if not self.machine_online:
            return
        with self.notify_client_errors():
            await self.session.process_controller.call(self.session.client.set_spindle_tool, 0, 0.0, installed=False)
            ui.notify("Spindle tool removed", type="positive")
