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

from gui.scene.machine_scene_view import MachineSceneView
from gui.views.atc_tab import AtcTabView
from gui.views.comms_log_tab import CommsLogTabView
from gui.views.eeprom_panel import EepromPanelView
from gui.views.environment_tab import EnvironmentTabView
from gui.views.gpio_tab import GpioTabView
from gui.views.io_panel import IoPanelView
from gui.views.machine_tab import FirmwareStateView, MachineTabView
from gui.views.machine_status import AtcPanelView, AxisPanelView
from gui.views.signals_tab import SignalsTabView
from gui.views.transport_panel import TransportPanelView


@dataclass
class HeaderView:
    model_select: Any | None = None
    cad_models_switch: Any | None = None
    power_switch: Any | None = None
    main_button_control: Any | None = None
    e_stop_switch: Any | None = None
    axis_readouts: dict[str, Any] = field(default_factory=dict)


@dataclass
class AppView:
    header: HeaderView = field(default_factory=HeaderView)
    machine_scene_view: MachineSceneView | None = None
    io_panel_view: IoPanelView | None = None
    axis_panel_view: AxisPanelView | None = None
    atc_panel_view: AtcPanelView | None = None
    transport_panel_view: TransportPanelView | None = None
    comms_log_view: CommsLogTabView | None = None
    eeprom_panel_view: EepromPanelView | None = None
    machine_tab_view: MachineTabView | None = None
    signals_tab_view: SignalsTabView | None = None
    environment_tab_view: EnvironmentTabView | None = None
    atc_tab_view: AtcTabView | None = None
    stock_tab_view: GpioTabView | None = None
    firmware_state_view: FirmwareStateView | None = None
    transport_log_cursor: int = 0
    telemetry_cursor: int = 0
    snapshot_cursor: int = 0
    physical_io_cursor: int = 0

    @property
    def model_select(self) -> Any | None:
        return self.header.model_select

    @property
    def cad_models_switch(self) -> Any | None:
        return self.header.cad_models_switch

    @property
    def power_switch(self) -> Any | None:
        return self.header.power_switch

    @property
    def main_button_control(self) -> Any | None:
        return self.header.main_button_control

    @property
    def rotary_accessory_switch(self) -> Any | None:
        return self.machine_tab_view.rotary_accessory_switch if self.machine_tab_view is not None else None

    @property
    def axis_readouts(self) -> dict[str, Any]:
        return self.header.axis_readouts

    @property
    def axis_detail_rows(self) -> dict[str, list[Any]]:
        return self.machine_tab_view.axis_detail_rows if self.machine_tab_view is not None else {}

    @property
    def axis_detail_labels(self) -> dict[str, dict[str, Any]]:
        return self.machine_tab_view.axis_detail_labels if self.machine_tab_view is not None else {}

    @property
    def axis_endstop_badges(self) -> dict[str, Any]:
        return self.machine_tab_view.axis_endstop_badges if self.machine_tab_view is not None else {}

    @property
    def front_panel_controls(self) -> dict[str, Any]:
        controls = dict(self.machine_tab_view.front_panel_controls) if self.machine_tab_view is not None else {}
        if self.header.e_stop_switch is not None:
            controls["e_stop"] = self.header.e_stop_switch
        return controls

    @property
    def front_panel_badges(self) -> dict[str, Any]:
        badges = dict(self.machine_tab_view.front_panel_badges) if self.machine_tab_view is not None else {}
        if self.signals_tab_view is not None:
            badges.update(self.signals_tab_view.front_panel_badges)
        return badges

    @property
    def firmware_switch_controls(self) -> dict[str, dict[str, Any]]:
        return self.signals_tab_view.firmware_switch_controls if self.signals_tab_view is not None else {}

    @property
    def laser_badges(self) -> dict[str, Any]:
        return self.signals_tab_view.laser_badges if self.signals_tab_view is not None else {}

    @property
    def pwm_labels(self) -> dict[str, Any]:
        return self.signals_tab_view.pwm_labels if self.signals_tab_view is not None else {}

    @property
    def motor_alarm_switches(self) -> dict[str, Any]:
        return self.environment_tab_view.motor_alarm_switches if self.environment_tab_view is not None else {}

    @property
    def motor_alarm_badges(self) -> dict[str, Any]:
        return self.environment_tab_view.motor_alarm_badges if self.environment_tab_view is not None else {}

    @property
    def temperature_sensor(self) -> Any | None:
        return self.environment_tab_view.temperature_sensor if self.environment_tab_view is not None else None

    @property
    def temperature_celsius(self) -> Any | None:
        return self.environment_tab_view.temperature_celsius if self.environment_tab_view is not None else None

    @property
    def temperature_status(self) -> Any | None:
        return self.environment_tab_view.temperature_status if self.environment_tab_view is not None else None

    @property
    def realtime_speed(self) -> Any | None:
        return self.environment_tab_view.realtime_speed if self.environment_tab_view is not None else None

    @property
    def tool_rows(self) -> dict[int, dict[str, Any]]:
        return self.atc_tab_view.tool_rows if self.atc_tab_view is not None else {}

    @property
    def box_controls(self) -> dict[str, dict[str, Any]]:
        return self.stock_tab_view.box_controls if self.stock_tab_view is not None else {}

    @property
    def pin_badges(self) -> dict[str, Any]:
        return self.stock_tab_view.pin_badges if self.stock_tab_view is not None else {}
