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

from collections import deque
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from typing import Any

from gui.core.defaults import PinWatch
from gui.protocol.model import PhysicalIoState

from .ui_helpers import set_badge, set_badge_na


def _rgb_components(rgb: Any) -> tuple[int, int, int]:
    return (
        max(0, min(255, int(getattr(rgb, "r", 0)))),
        max(0, min(255, int(getattr(rgb, "g", 0)))),
        max(0, min(255, int(getattr(rgb, "b", 0)))),
    )


def rgb_css(rgb: Any) -> str:
    r, g, b = _rgb_components(rgb)
    return f"rgb({r}, {g}, {b})"


def front_panel_led_text(machine_model: str, front_panel: Any | None = None) -> tuple[str, str]:
    if machine_model == "ca1":
        if front_panel is not None and getattr(front_panel, "led_strip_available", False):
            return ("CA1 LED strip", "active")
        return ("CA1 LED strip", "not available")
    if front_panel is not None and getattr(front_panel, "direct_rgb_available", False):
        rgb = front_panel.direct_rgb
        return ("C1 RGB LED", f"R{rgb.r:03d} G{rgb.g:03d} B{rgb.b:03d}")
    return ("C1 RGB LED", "not wired" if front_panel is not None else "not available")


def c1_main_button_style(front_panel: Any | None) -> str:
    if front_panel is None or not getattr(front_panel, "direct_rgb_available", False):
        return ""
    color = rgb_css(front_panel.direct_rgb)
    return (
        f"background-color: {color}; border-color: {color}; color: #111827; "
        f"box-shadow: 0 0 0 2px color-mix(in srgb, {color} 35%, transparent);"
    )


def ca1_led_strip_styles(front_panel: Any | None) -> list[str]:
    if front_panel is None or not getattr(front_panel, "led_strip_available", False):
        return ["background: #cbd5e1;" for _ in range(5)]
    return [f"background: {rgb_css(segment)};" for segment in front_panel.led_strip]


def pwm_status_text(pwm: Any) -> str:
    return f"{pwm.duty * 100:5.1f}% / {pwm.period_us:.0f}us" if pwm.configured else "not configured"


@dataclass
class IoPanelView:
    machine_model_value: Callable[[], str]
    pwm_watches: Sequence[PinWatch]
    probe_badge: Any
    tool_setter_badge: Any
    cover_switch: Any
    cover_badge: Any
    temperature_status: Any
    front_panel_controls: dict[str, Any]
    front_panel_badges: dict[str, Any]
    motor_alarm_switches: dict[str, Any]
    motor_alarm_badges: dict[str, Any]
    spindle_alarm_switch: Any
    spindle_alarm_badge: Any
    firmware_switch_controls: dict[str, dict[str, Any]]
    laser_badges: dict[str, Any]
    laser_power_label: Any
    pwm_labels: dict[str, Any]
    main_button_control: Any | None = None
    ca1_led_strip_indicators: Sequence[Any] = ()
    # Values we wrote into two-way controls whose change events have not been
    # handled yet, in write order per control. See mirror_control().
    echoed_writes: dict[str, deque[bool]] = field(default_factory=dict)

    def mirror_control(self, name: str, control: Any, value: bool) -> None:
        """Show simulator state in a two-way control without commanding it back.

        Writing a control's value fires its change handler just like a click
        does, and an async handler runs as a background task well after this
        call returns - so a re-entrancy flag, cleared synchronously, can never
        mark the difference. Record the value instead and let the handler claim
        it via consume_echoed_write().
        """
        if control is None or bool(control.value) == value:
            return  # an unchanged assignment fires no handler, so owe nothing
        self.echoed_writes.setdefault(name, deque()).append(value)
        control.value = value

    def consume_echoed_write(self, name: str, value: bool) -> bool:
        """True when this change event is one of our own writes, not the user's.

        Matching on the value keeps a click that was queued *before* our write
        from claiming our echo: a click always carries the opposite of what the
        control held, so it can never match the oldest value we wrote.
        """
        pending = self.echoed_writes.get(name)
        if not pending or pending[0] != value:
            return False
        pending.popleft()
        return True

    def apply_snapshot(self, snapshot: PhysicalIoState) -> None:
        set_badge(self.probe_badge, snapshot.probe_contact, "contact", "open", warn=True)
        set_badge(self.tool_setter_badge, snapshot.tool_setter_contact, "contact", "open", warn=True)
        self.mirror_control("cover", self.cover_switch, snapshot.cover_open)
        set_badge(self.cover_badge, snapshot.cover_open, "open", "closed", warn=True)

        self._apply_front_panel(snapshot.front_panel)
        for axis, triggered in snapshot.motor_alarms.items():
            if axis in self.motor_alarm_badges:
                set_badge(self.motor_alarm_badges[axis], triggered, "alarm", "clear", warn=True)

        if snapshot.spindle_alarm_available:
            for badge in self._spindle_alarm_badges():
                set_badge(badge, snapshot.spindle_alarm_triggered, "alarm", "clear", warn=True)
        else:
            for badge in self._spindle_alarm_badges():
                set_badge_na(badge)

        for name, controls in self.firmware_switch_controls.items():
            state = snapshot.switches.get(name)
            if state is None or not state.available:
                controls["readback"].text = "n/a"
                set_badge(controls["badge"], False, "on", "off")
                continue
            controls["readback"].text = f"{state.value:.1f}"
            set_badge(controls["badge"], state.on, "on", "off")

        self._apply_laser(snapshot.laser)
        for name, port, pin in self.pwm_watches:
            pwm = snapshot.pwm_outputs.get((port, pin))
            if pwm is not None and name in self.pwm_labels:
                self.pwm_labels[name].text = pwm_status_text(pwm)

    def reset(self, machine_model: str) -> None:
        for control in self.front_panel_controls.values():
            if hasattr(control, "value"):
                control.value = False
        if hasattr(self.spindle_alarm_switch, "value"):
            self.spindle_alarm_switch.value = False
        for badge in self._spindle_alarm_badges():
            set_badge(badge, False, "alarm", "clear", warn=True)
        for name, badge in self.front_panel_badges.items():
            if name == "led_strip":
                continue
            if name == "led_name":
                badge.text = front_panel_led_text(machine_model)[0]
            else:
                badge.text = front_panel_led_text(machine_model)[1] if name == "rgb" else "off"
            badge.classes(remove="badge-on badge-off badge-warn")
            if name not in {"rgb", "led_name"}:
                badge.classes(add="badge-off")
        self._apply_main_button_led(None)
        self._apply_ca1_led_strip(None)
        for controls in self.firmware_switch_controls.values():
            controls["readback"].text = "n/a"
            set_badge(controls["badge"], False, "on", "off")
        for badge in self.laser_badges.values():
            set_badge(badge, False, "on", "off")
        self.laser_power_label.text = "n/a"

    def set_temperature_status(self, celsius: float) -> None:
        self.temperature_status.text = f"{celsius:.1f} C"

    def _apply_front_panel(self, front_panel: Any) -> None:
        if "main_button" in self.front_panel_badges:
            set_badge(
                self.front_panel_badges["main_button"], bool(front_panel.main_button_pressed), "pressed", "released"
            )
        if "e_stop" in self.front_panel_badges:
            set_badge(
                self.front_panel_badges["e_stop"], bool(front_panel.e_stop_pressed), "pressed", "released", warn=True
            )
        if "v12" in self.front_panel_badges:
            set_badge(self.front_panel_badges["v12"], bool(front_panel.power_rails.v12), "on", "off")
        if "v24" in self.front_panel_badges:
            set_badge(self.front_panel_badges["v24"], bool(front_panel.power_rails.v24), "on", "off")
        led_name, led_value = front_panel_led_text(self.machine_model_value(), front_panel)
        if "led_name" in self.front_panel_badges:
            self.front_panel_badges["led_name"].text = led_name
        if "rgb" in self.front_panel_badges:
            self.front_panel_badges["rgb"].text = led_value
        self._apply_main_button_led(front_panel)
        self._apply_ca1_led_strip(front_panel)

    def _apply_main_button_led(self, front_panel: Any | None) -> None:
        if self.main_button_control is None:
            return
        style = c1_main_button_style(front_panel)
        if style:
            self.main_button_control.style(replace=style)
        else:
            self.main_button_control.style(replace="")

    def _apply_ca1_led_strip(self, front_panel: Any | None) -> None:
        for indicator, style in zip(self.ca1_led_strip_indicators, ca1_led_strip_styles(front_panel), strict=False):
            indicator.style(replace=style)

    def _spindle_alarm_badges(self) -> list[Any]:
        if isinstance(self.spindle_alarm_badge, list):
            return self.spindle_alarm_badge
        return [self.spindle_alarm_badge]

    def _apply_laser(self, state: Any) -> None:
        if not state.available:
            for badge in self.laser_badges.values():
                set_badge_na(badge)
            self.laser_power_label.text = "n/a"
            return
        set_badge(self.laser_badges["mode"], bool(state.mode), "laser", "cnc")
        set_badge(self.laser_badges["firing"], bool(state.firing), "on", "off", warn=True)
        set_badge(self.laser_badges["testing"], bool(state.testing), "test", "idle", warn=True)
        self.laser_power_label.text = f"{state.power_percent:.1f}% / {state.scale_percent:.1f}%"
