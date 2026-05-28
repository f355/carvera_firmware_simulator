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

from types import SimpleNamespace

from gui.app_view import AppView, HeaderView
from gui.views.atc_tab import AtcTabView
from gui.views.environment_tab import EnvironmentTabView
from gui.views.gpio_tab import GpioTabView
from gui.views.machine_tab import MachineTabView
from gui.views.signals_tab import SignalsTabView


def test_app_view_aggregates_typed_subviews_without_copy_bags() -> None:
    view = AppView(header=HeaderView())
    view.header.e_stop_switch = SimpleNamespace(value=False)
    view.header.axis_readouts["X"] = SimpleNamespace(name="x-dro")
    view.machine_tab_view = MachineTabView()
    view.machine_tab_view.front_panel_controls["cover"] = SimpleNamespace(value=True)
    view.machine_tab_view.axis_detail_labels["X"] = {"physical": SimpleNamespace(text="")}
    view.machine_tab_view.axis_endstop_badges["X"] = SimpleNamespace(text="")
    view.machine_tab_view.axis_detail_rows["X"] = [SimpleNamespace(visible=True)]
    view.signals_tab_view = SignalsTabView()
    view.signals_tab_view.front_panel_badges["v12"] = SimpleNamespace(text="")
    view.signals_tab_view.firmware_switch_controls["fan"] = {"badge": SimpleNamespace(text="")}
    view.signals_tab_view.laser_badges["mode"] = SimpleNamespace(text="")
    view.signals_tab_view.pwm_labels["spindle"] = SimpleNamespace(text="")
    view.environment_tab_view = EnvironmentTabView()
    view.environment_tab_view.motor_alarm_switches["X"] = SimpleNamespace(value=False)
    view.environment_tab_view.motor_alarm_badges["X"] = SimpleNamespace(text="")
    view.environment_tab_view.temperature_sensor = SimpleNamespace(value="spindle")
    view.environment_tab_view.temperature_celsius = SimpleNamespace(value=25.0)
    view.environment_tab_view.temperature_status = SimpleNamespace(text="")
    view.stock_tab_view = GpioTabView()
    view.stock_tab_view.box_controls["stock"] = {"enabled": SimpleNamespace(value=True)}
    view.stock_tab_view.pin_badges["unused"] = SimpleNamespace(text="")
    view.atc_tab_view = AtcTabView(
        available_badge=SimpleNamespace(text=""),
        active_tool_label=SimpleNamespace(text=""),
        target_tool_label=SimpleNamespace(text=""),
        tlo_label=SimpleNamespace(text=""),
        held_length_label=SimpleNamespace(text=""),
    )
    view.atc_tab_view.tool_rows[1] = {"occupied": SimpleNamespace(value=True)}

    assert view.power_switch is None
    assert view.front_panel_controls["e_stop"] is view.header.e_stop_switch
    assert view.front_panel_controls["cover"].value is True
    assert view.front_panel_badges["v12"].text == ""
    assert view.axis_readouts["X"].name == "x-dro"
    assert view.axis_detail_labels["X"]["physical"].text == ""
    assert view.axis_endstop_badges["X"].text == ""
    assert view.firmware_switch_controls["fan"]["badge"].text == ""
    assert view.laser_badges["mode"].text == ""
    assert view.pwm_labels["spindle"].text == ""
    assert view.motor_alarm_switches["X"].value is False
    assert view.motor_alarm_badges["X"].text == ""
    assert view.box_controls["stock"]["enabled"].value is True
    assert view.pin_badges["unused"].text == ""
    assert view.tool_rows[1]["occupied"].value is True
    assert view.temperature_sensor is not None
    assert view.temperature_celsius is not None
    assert view.temperature_status is not None
    assert view.temperature_sensor.value == "spindle"
    assert view.temperature_celsius.value == 25.0
    assert view.temperature_status.text == ""
