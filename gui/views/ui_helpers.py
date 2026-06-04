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

from typing import Any, Protocol


class BadgeLike(Protocol):
    text: str

    def classes(self, *, add: str | None = None, remove: str | None = None) -> Any: ...


class LockableControl(Protocol):
    def disable(self) -> Any: ...

    def enable(self) -> Any: ...


def event_bool(event: Any) -> bool:
    value = getattr(event, "value", event)
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return bool(value)


def event_value(event: Any) -> Any:
    if hasattr(event, "value"):
        return event.value
    if hasattr(event, "args"):
        args = event.args
        if isinstance(args, (list, tuple)) and len(args) == 1:
            return args[0]
        return args
    return event


def set_badge(
    label: BadgeLike, active: bool, on_text: str = "ON", off_text: str = "OFF", *, warn: bool = False
) -> None:
    label.text = on_text if active else off_text
    label.classes(remove="badge-on badge-off badge-warn")
    if warn and active:
        label.classes(add="badge-warn")
    else:
        label.classes(add="badge-on" if active else "badge-off")


def set_status_badge(label: BadgeLike, good: bool, good_text: str, bad_text: str) -> None:
    label.text = good_text if good else bad_text
    label.classes(remove="badge-on badge-off badge-warn")
    label.classes(add="badge-on" if good else "badge-warn")


def set_control_locked(control: LockableControl, locked: bool) -> None:
    if locked:
        control.disable()
    else:
        control.enable()
