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
from gui.core.session import SimulatorSession
from gui.protocol.model import InteractiveTransportState, MachineState, ToolKind, pb, snapshot_to_state
from gui.protocol.sim_client import SimulatorClientError
from gui.scene.lighting import (
    DEFAULT_MODEL_COLOR,
    ModelMaterialSettings,
    SceneLightingSettings,
    apply_scene_lighting,
)
from gui.views.tool_table import (
    clear_tool_occupancy,
    collect_box_values,
    collect_tool_table,
    inferred_tool_kind,
    load_default_tools as load_default_tool_controls,
    physical_length_from_stickout,
)
from gui.views.ui_helpers import event_bool, set_control_locked, set_status_badge


class AppActions:
    def __init__(self, session: SimulatorSession) -> None:
        self.session = session

    async def publish_snapshot(self, view: AppView, snapshot: pb.MachineSnapshot) -> None:
        self.publish_machine_state(view, snapshot_to_state(snapshot))

    def publish_machine_state(self, view: AppView, state: MachineState) -> None:
        assert view.firmware_state_view is not None
        merged_state = self.session.state_store.apply_full_snapshot(state).machine_state
        assert merged_state is not None
        state = merged_state
        set_status_badge(view.firmware_state_view.firmware_badge, state.firmware_booted, "booted", "off")
        set_status_badge(view.firmware_state_view.homed_badge, state.homed, "homed", "not homed")
        set_status_badge(view.firmware_state_view.soft_limit_badge, state.soft_endstop_enabled, "enabled", "disabled")
        if view.axis_panel_view is not None:
            view.axis_panel_view.update(state)
        if view.atc_panel_view is not None:
            view.atc_panel_view.update(state.atc)
        self.update_machine_scene(view, state)

    async def publish_transport(self, view: AppView, transport: InteractiveTransportState | None) -> None:
        self.session.state_store.set_online(transport=transport)
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

    async def power_on(self, view: AppView) -> None:
        assert view.power_switch is not None
        assert view.model_select is not None
        assert view.rotary_accessory_switch is not None
        gui_state = self.session.state_store.snapshot()
        if gui_state.power_transition:
            view.power_switch.value = gui_state.machine_online
            return
        self.session.state_store.set_power_transition(True)
        view.power_switch.disable()
        set_control_locked(view.model_select, True)
        try:
            result = await self.session.process_controller.power_on(
                machine_model=str(view.model_select.value),
                tools=collect_tool_table(view.tool_rows, rack_only=True),
                rotary_enabled=event_bool(view.rotary_accessory_switch),
            )
            await self.publish_transport(view, result.transport)
            if result.snapshot is not None:
                await self.publish_snapshot(view, result.snapshot)
            await self.apply_physical_boxes(view, notify=False)
            await self.refresh_eeprom(view, notify=False)
        except Exception as exc:
            self.session.state_store.mark_offline()
            view.power_switch.value = False
            ui.notify(str(exc), type="negative")
        finally:
            self.update_model_selector_lock(view)
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
        self.update_model_selector_lock(view)
        try:
            await self.session.process_controller.power_off()
            self.session.state_store.mark_offline()
            if view.axis_panel_view is not None:
                view.axis_panel_view.reset()
            if view.transport_panel_view is not None:
                view.transport_panel_view.reset()
            set_status_badge(view.firmware_state_view.firmware_badge, False, "booted", "off")
            set_status_badge(view.firmware_state_view.homed_badge, False, "homed", "not homed")
            set_status_badge(view.firmware_state_view.soft_limit_badge, False, "enabled", "disabled")
            if view.io_panel_view is not None:
                view.io_panel_view.reset(str(view.model_select.value))
            if view.atc_panel_view is not None:
                view.atc_panel_view.reset()
            if view.eeprom_panel_view is not None:
                view.eeprom_panel_view.status_label.text = "Power on and refresh to view named EEPROM fields."
            self.update_machine_scene(view, None)
        finally:
            self.update_model_selector_lock(view)
            view.power_switch.enable()

    async def power_changed(self, view: AppView, event: Any) -> None:
        if event_bool(event):
            await self.power_on(view)
        else:
            await self.power_off(view)

    async def apply_atc_table(self, view: AppView, *, notify: bool = True) -> None:
        tools = collect_tool_table(view.tool_rows, rack_only=True)
        if not self.session.state_store.snapshot().machine_online:
            if notify:
                ui.notify("Tool table will be applied when the simulator powers on.", type="info")
            return
        try:
            await self.session.process_controller.call(self.session.client.set_atc_pocket_tools, tools)
            if notify:
                ui.notify("ATC rack updated", type="positive")
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    def load_default_tools(self, view: AppView) -> None:
        load_default_tool_controls(view.tool_rows)

    def clear_tool_table(self, view: AppView) -> None:
        clear_tool_occupancy(view.tool_rows)

    def collect_box(self, view: AppView, name: str) -> tuple[float, float, float, float, float, float]:
        return collect_box_values(view.box_controls[name])

    async def apply_physical_boxes(self, view: AppView, *, notify: bool = True) -> None:
        if not self.session.state_store.snapshot().machine_online:
            if notify:
                ui.notify("Stock geometry will be applied when the simulator powers on.", type="info")
            return
        try:
            await self.session.process_controller.call(
                self.session.client.set_stock_box,
                self.collect_box(view, "stock"),
                enabled=event_bool(view.box_controls["stock"]["enabled"]),
            )
            if notify:
                ui.notify("Stock geometry updated", type="positive")
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def load_spindle_tool(
        self,
        view: AppView,
        tool: int,
        length_mm: float,
        *,
        kind: ToolKind | str = ToolKind.UNSPECIFIED,
        probe_tip_diameter_mm: float = 0.0,
    ) -> None:
        if not self.session.state_store.snapshot().machine_online:
            ui.notify("Power on the simulator before loading a spindle tool.", type="warning")
            return
        try:
            await self.session.process_controller.call(
                self.session.client.set_spindle_tool,
                tool,
                length_mm,
                installed=True,
                kind=kind,
                probe_tip_diameter_mm=probe_tip_diameter_mm,
            )
            ui.notify(f"Tool {tool} loaded in spindle", type="positive")
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

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
        if not self.session.state_store.snapshot().machine_online:
            return
        try:
            await self.session.process_controller.call(self.session.client.set_spindle_tool, 0, 0.0, installed=False)
            ui.notify("Spindle tool removed", type="positive")
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

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
        self.update_machine_shell_model(view)
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

    async def refresh_eeprom(self, view: AppView, *, notify: bool = True) -> None:
        if view.eeprom_panel_view is None:
            return
        if not self.session.state_store.snapshot().machine_online:
            view.eeprom_panel_view.status_label.text = "Power on and refresh to view named EEPROM fields."
            if notify:
                ui.notify("Power on the simulator before reading EEPROM.", type="warning")
            return
        try:
            fields = await self.session.process_controller.call(self.session.client.get_eeprom_fields)
            view.eeprom_panel_view.set_fields(fields)
            if notify:
                ui.notify("EEPROM fields refreshed", type="positive")
        except SimulatorClientError as exc:
            ui.notify(str(exc), type="negative")

    async def write_eeprom(self, view: AppView) -> None:
        if view.eeprom_panel_view is None:
            return
        if not self.session.state_store.snapshot().machine_online:
            ui.notify("Power on the simulator before writing EEPROM.", type="warning")
            return
        try:
            await self.session.process_controller.call(
                self.session.client.set_eeprom_fields, view.eeprom_panel_view.edited_fields()
            )
            await self.refresh_eeprom(view, notify=False)
            ui.notify("EEPROM fields written; reset firmware to reload running modules.", type="positive")
        except (SimulatorClientError, TypeError, ValueError) as exc:
            ui.notify(str(exc), type="negative")

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

    def scene_appearance_changed(self, view: AppView, _event: Any = None) -> None:
        if view.machine_scene_view is None:
            return
        apply_scene_lighting(view.machine_scene_view.scene, self.current_lighting_settings(view))
        view.machine_scene_view.apply_model_material(self.current_material_settings(view))

    def current_lighting_settings(self, view: AppView) -> SceneLightingSettings:
        controls = view.environment_tab_view.lighting_controls if view.environment_tab_view is not None else {}
        defaults = SceneLightingSettings()
        return SceneLightingSettings(
            ambient=self.control_float(controls, "ambient", defaults.ambient),
            key=self.control_float(controls, "key", defaults.key),
            key_x=self.control_float(controls, "key_x", defaults.key_x),
            key_y=self.control_float(controls, "key_y", defaults.key_y),
            key_z=self.control_float(controls, "key_z", defaults.key_z),
            fill=self.control_float(controls, "fill", defaults.fill),
            fill_x=self.control_float(controls, "fill_x", defaults.fill_x),
            fill_y=self.control_float(controls, "fill_y", defaults.fill_y),
            fill_z=self.control_float(controls, "fill_z", defaults.fill_z),
            exposure=self.control_float(controls, "exposure", defaults.exposure),
            shadows=event_bool(controls["shadows"]) if "shadows" in controls else defaults.shadows,
            shadow_radius=self.control_float(controls, "shadow_radius", defaults.shadow_radius),
            shadow_bias=self.control_float(controls, "shadow_bias", defaults.shadow_bias),
            shadow_map_size=self.control_float(controls, "shadow_map_size", defaults.shadow_map_size),
        )

    def current_material_settings(self, view: AppView) -> ModelMaterialSettings:
        controls = view.environment_tab_view.material_controls if view.environment_tab_view is not None else {}
        defaults = ModelMaterialSettings()
        color = str(controls["color"].value) if "color" in controls and controls["color"].value else DEFAULT_MODEL_COLOR
        return ModelMaterialSettings(
            color=color,
            opacity=self.control_float(controls, "opacity", defaults.opacity),
            roughness=self.control_float(controls, "roughness", defaults.roughness),
            metalness=self.control_float(controls, "metalness", defaults.metalness),
        )

    @staticmethod
    def control_float(controls: dict[str, Any], name: str, default: float) -> float:
        control = controls.get(name)
        if control is None:
            return default
        try:
            return float(control.value)
        except (TypeError, ValueError):
            return default
