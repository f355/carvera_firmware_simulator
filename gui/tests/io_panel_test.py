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

from gui.protocol.model import PhysicalIoState
from gui.tests.fakes import FakeLabel, FakeStyleControl
from gui.views.io_panel import (
    IoPanelView,
    ca1_led_strip_styles,
    c1_main_button_style,
    front_panel_led_text,
    pwm_status_text,
)


def make_io_panel_view(main_button_control: FakeStyleControl | None = None) -> IoPanelView:
    return IoPanelView(
        machine_model_value=lambda: "c1",
        pwm_watches=(),
        probe_badge=FakeLabel(),
        tool_setter_badge=FakeLabel(),
        cover_switch=SimpleNamespace(value=False),
        cover_badge=FakeLabel(),
        temperature_status=FakeLabel(),
        front_panel_controls={"e_stop": SimpleNamespace(value=True)},
        front_panel_badges={
            "led_name": FakeLabel(),
            "rgb": FakeLabel(),
            "main_button": FakeLabel(),
            "led_strip": [FakeStyleControl() for _ in range(5)],
        },
        motor_alarm_switches={},
        motor_alarm_badges={},
        spindle_alarm_switch=SimpleNamespace(value=True),
        spindle_alarm_badge=FakeLabel(),
        firmware_switch_controls={},
        laser_badges={},
        laser_power_label=FakeLabel(),
        pin_badges={},
        pwm_labels={},
        main_button_control=main_button_control,
        ca1_led_strip_indicators=(),
    )


def test_front_panel_helpers_format_led_and_pwm_state() -> None:
    assert front_panel_led_text("ca1") == ("CA1 LED strip", "not available")
    assert front_panel_led_text("c1") == ("C1 RGB LED", "not available")

    rgb = SimpleNamespace(r=3, g=25, b=255)
    front_panel = SimpleNamespace(direct_rgb_available=True, direct_rgb=rgb)
    assert front_panel_led_text("c1", front_panel) == ("C1 RGB LED", "R003 G025 B255")

    unavailable = SimpleNamespace(direct_rgb_available=False)
    assert front_panel_led_text("c1", unavailable) == ("C1 RGB LED", "not wired")

    c1_panel = SimpleNamespace(direct_rgb_available=True, direct_rgb=SimpleNamespace(r=0, g=255, b=128))
    assert "rgb(0, 255, 128)" in c1_main_button_style(c1_panel)

    strip_panel = SimpleNamespace(
        led_strip_available=True,
        led_strip=[SimpleNamespace(r=0, g=104, b=0), SimpleNamespace(r=104, g=0, b=0)],
    )
    strip_styles = ca1_led_strip_styles(strip_panel)
    assert strip_styles == [
        "background: rgb(0, 104, 0);",
        "background: rgb(104, 0, 0);",
    ]

    configured_pwm = SimpleNamespace(configured=True, duty=0.375, period_us=50)
    assert pwm_status_text(configured_pwm) == " 37.5% / 50us"
    unconfigured_pwm = SimpleNamespace(configured=False, duty=0.0, period_us=0)
    assert pwm_status_text(unconfigured_pwm) == "not configured"


def test_io_panel_applies_and_resets_main_button_led() -> None:
    c1_panel = SimpleNamespace(direct_rgb_available=True, direct_rgb=SimpleNamespace(r=0, g=255, b=128))
    control = FakeStyleControl()
    c1_panel_view = make_io_panel_view(control)
    c1_panel_view._apply_main_button_led(c1_panel)
    assert "rgb(0, 255, 128)" in control.style_text

    reset_view = make_io_panel_view()
    reset_view.reset("ca1")
    assert reset_view.front_panel_badges["rgb"].text == "not available"


def test_front_panel_snapshot_does_not_overwrite_physical_e_stop_control() -> None:
    front_panel = SimpleNamespace(
        main_button_pressed=False,
        e_stop_pressed=False,
        power_rails=SimpleNamespace(v12=True, v24=True),
        direct_rgb_available=False,
        led_strip_available=False,
        led_strip=[],
    )
    snapshot = PhysicalIoState(
        probe_contact=False,
        tool_setter_contact=False,
        cover_open=False,
        front_panel=front_panel,
        motor_alarms={},
        spindle_alarm_available=False,
        spindle_alarm_triggered=False,
        switches={},
        laser=SimpleNamespace(available=False),
        pwm_outputs={},
    )

    view = make_io_panel_view()
    view.front_panel_controls["e_stop"].value = True

    view.apply_snapshot(snapshot)

    assert view.front_panel_controls["e_stop"].value is True


def test_io_snapshot_does_not_overwrite_fault_injection_switches() -> None:
    front_panel = SimpleNamespace(
        main_button_pressed=False,
        e_stop_pressed=False,
        power_rails=SimpleNamespace(v12=True, v24=True),
        direct_rgb_available=False,
        led_strip_available=False,
        led_strip=[],
    )
    snapshot = PhysicalIoState(
        probe_contact=False,
        tool_setter_contact=False,
        cover_open=False,
        front_panel=front_panel,
        motor_alarms={"X": True, "Y": True, "Z": True},
        spindle_alarm_available=True,
        spindle_alarm_triggered=True,
        switches={},
        laser=SimpleNamespace(available=False),
        pwm_outputs={},
    )
    view = make_io_panel_view()
    view.motor_alarm_switches = {
        "X": SimpleNamespace(value=False),
        "Y": SimpleNamespace(value=False),
        "Z": SimpleNamespace(value=False),
    }
    view.motor_alarm_badges = {"X": FakeLabel(), "Y": FakeLabel(), "Z": FakeLabel()}
    view.spindle_alarm_switch.value = False

    view.apply_snapshot(snapshot)

    assert {axis: control.value for axis, control in view.motor_alarm_switches.items()} == {
        "X": False,
        "Y": False,
        "Z": False,
    }
    assert view.spindle_alarm_switch.value is False
    assert view.motor_alarm_badges["X"].text == "alarm"
    assert view.spindle_alarm_badge.text == "alarm"
