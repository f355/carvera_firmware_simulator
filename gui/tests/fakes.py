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

import itertools

_fake_ids = itertools.count(1)


class FakeObject:
    def __init__(self) -> None:
        self.id = next(_fake_ids)
        self.last_move: tuple[float, float, float] | None = None
        self.visibility: list[bool] = []
        self.materials: list[object] = []
        self.deleted = False

    def move(self, x: float, y: float, z: float) -> FakeObject:
        self.last_move = (x, y, z)
        return self

    def scale(self, _value: float) -> FakeObject:
        return self

    def rotate(self, *_values: float) -> FakeObject:
        return self

    def material(self, *values: object) -> FakeObject:
        self.materials.append(values[0] if len(values) == 1 else values)
        return self

    def visible(self, value: bool) -> FakeObject:
        self.visibility.append(value)
        return self

    def delete(self) -> None:
        self.deleted = True


class FakeLine(FakeObject):
    def __init__(self, start: list[float], end: list[float]) -> None:
        super().__init__()
        self.start = start
        self.end = end
        self.color = ""

    def material(self, *values: object) -> FakeLine:
        super().material(*values)
        if len(values) == 1 and isinstance(values[0], str):
            self.color = values[0]
        return self


class FakeScene:
    def __init__(self) -> None:
        self.id = next(_fake_ids)
        self.lines: list[FakeLine] = []
        self.objects: list[FakeObject] = []
        self.cylinders: list[FakeObject] = []
        self.cylinder_kwargs: list[dict[str, object]] = []
        self.boxes: list[FakeObject] = []

    def line(self, start: list[float], end: list[float]) -> FakeLine:
        line = FakeLine(start, end)
        self.lines.append(line)
        return line

    def gltf(self, _url: str) -> FakeObject:
        model = FakeObject()
        self.objects.append(model)
        return model

    def cylinder(self, **kwargs: object) -> FakeObject:
        model = FakeObject()
        self.cylinders.append(model)
        self.cylinder_kwargs.append(kwargs)
        return model

    def box(self, **_kwargs: object) -> FakeObject:
        model = FakeObject()
        self.boxes.append(model)
        return model


class FakeLabel:
    def __init__(self, text: str = "") -> None:
        self.text = text
        self.classes_text = ""
        self.classes_added: list[str] = []
        self.classes_removed: list[str] = []

    def classes(self, *, add: str | None = None, remove: str | None = None) -> None:
        if remove is not None:
            self.classes_removed.append(remove)
            removed = set(remove.split())
            self.classes_text = " ".join(part for part in self.classes_text.split() if part not in removed)
        if add is not None:
            self.classes_added.append(add)
            existing = self.classes_text.split()
            for part in add.split():
                if part not in existing:
                    existing.append(part)
            self.classes_text = " ".join(existing)


class FakeControl:
    def __init__(self, value: object = None) -> None:
        self.value = value
        self.disabled = False

    def disable(self) -> None:
        self.disabled = True

    def enable(self) -> None:
        self.disabled = False


class FakeStyleControl:
    def __init__(self) -> None:
        self.style_text = ""

    def style(self, text: str = "", *, replace: str | None = None) -> None:
        if replace is not None and not isinstance(replace, str):
            raise AttributeError(f"'{type(replace).__name__}' object has no attribute 'split'")
        self.style_text = replace if replace is not None else text
