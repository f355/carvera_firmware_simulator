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

from gui.views.ui_helpers import event_bool, set_badge, set_control_locked, set_status_badge


class FakeLabel:
    def __init__(self) -> None:
        self.text = ""
        self.classes_text = ""

    def classes(self, *, add: str | None = None, remove: str | None = None) -> None:
        if remove is not None:
            removed = set(remove.split())
            self.classes_text = " ".join(part for part in self.classes_text.split() if part not in removed)
        if add is not None:
            existing = self.classes_text.split()
            for part in add.split():
                if part not in existing:
                    existing.append(part)
            self.classes_text = " ".join(existing)


class FakeControl:
    def __init__(self) -> None:
        self.disabled = False

    def disable(self) -> None:
        self.disabled = True

    def enable(self) -> None:
        self.disabled = False


def test_ui_helpers_test() -> None:
    label = FakeLabel()
    set_badge(label, True, "HIT", "clear", warn=True)
    if label.text != "HIT" or "badge-warn" not in label.classes_text:
        raise SystemExit("warn badge should use warning class when active")

    set_badge(label, False, "HIT", "clear", warn=True)
    if label.text != "clear" or label.classes_text != "badge-off":
        raise SystemExit("inactive badge should use off class")

    set_status_badge(label, True, "booted", "off")
    if label.text != "booted" or label.classes_text != "badge-on":
        raise SystemExit("good status should use on class")

    set_status_badge(label, False, "booted", "off")
    if label.text != "off" or label.classes_text != "badge-warn":
        raise SystemExit("bad status should use warning class")

    control = FakeControl()
    set_control_locked(control, True)
    if not control.disabled:
        raise SystemExit("locked control should be disabled")

    set_control_locked(control, False)
    if control.disabled:
        raise SystemExit("unlocked control should be enabled")

    if event_bool(SimpleNamespace(value="false")):
        raise SystemExit("string false event values should be false")
    if not event_bool(SimpleNamespace(value="true")):
        raise SystemExit("string true event values should be true")
    if event_bool(SimpleNamespace(value=0)):
        raise SystemExit("zero event values should be false")
    if not event_bool(SimpleNamespace(value=1)):
        raise SystemExit("nonzero event values should be true")
