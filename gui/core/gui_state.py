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

from dataclasses import dataclass, replace
from threading import Lock

from gui.protocol.model import InteractiveTransportState, MachineState


@dataclass(frozen=True, slots=True)
class GuiStateSnapshot:
    machine_online: bool = False
    power_transition: bool = False
    machine_model: str | None = None
    transport: InteractiveTransportState | None = None
    machine_state: MachineState | None = None


class GuiStateStore:
    def __init__(self) -> None:
        self._lock = Lock()
        self._version = 0
        self._snapshot = GuiStateSnapshot()

    def snapshot(self) -> GuiStateSnapshot:
        with self._lock:
            return self._snapshot

    def latest_since(self, cursor: int) -> tuple[int, GuiStateSnapshot | None]:
        with self._lock:
            if cursor == self._version:
                return cursor, None
            return self._version, self._snapshot

    def set_power_transition(self, active: bool, *, machine_model: str | None = None) -> GuiStateSnapshot:
        with self._lock:
            self._snapshot = replace(
                self._snapshot,
                power_transition=active,
                machine_model=machine_model if machine_model is not None else self._snapshot.machine_model,
            )
            self._version += 1
            return self._snapshot

    def set_online(
        self, *, transport: InteractiveTransportState | None, machine_model: str | None = None
    ) -> GuiStateSnapshot:
        with self._lock:
            self._snapshot = replace(
                self._snapshot,
                machine_online=True,
                power_transition=False,
                machine_model=machine_model if machine_model is not None else self._snapshot.machine_model,
                transport=transport,
            )
            self._version += 1
            return self._snapshot

    def set_machine_state(self, machine_state: MachineState) -> GuiStateSnapshot:
        with self._lock:
            self._snapshot = replace(self._snapshot, machine_state=machine_state)
            self._version += 1
            return self._snapshot

    def apply_full_snapshot(self, full_snapshot: MachineState) -> GuiStateSnapshot:
        with self._lock:
            previous = self._snapshot.machine_state
            if previous is None:
                merged = full_snapshot
            else:
                # Fast telemetry is the authoritative source for physical stepper positions.
                # Full snapshots arrive through an independent slower buffer and can otherwise
                # momentarily move the scene back to an older sample.
                merged = MachineState(
                    firmware_booted=full_snapshot.firmware_booted,
                    homed=full_snapshot.homed,
                    soft_endstop_enabled=full_snapshot.soft_endstop_enabled,
                    work_area=full_snapshot.work_area,
                    physical_travel=full_snapshot.physical_travel or previous.physical_travel,
                    axes=previous.axes or full_snapshot.axes,
                    atc=full_snapshot.atc or previous.atc,
                    spindle=full_snapshot.spindle or previous.spindle,
                    tool_setter=full_snapshot.tool_setter,
                )
            self._snapshot = replace(self._snapshot, machine_state=merged)
            self._version += 1
            return self._snapshot

    def apply_telemetry(self, telemetry: MachineState) -> GuiStateSnapshot:
        with self._lock:
            previous = self._snapshot.machine_state
            if previous is None:
                merged = telemetry
            else:
                merged = MachineState(
                    firmware_booted=telemetry.firmware_booted,
                    homed=telemetry.homed,
                    soft_endstop_enabled=previous.soft_endstop_enabled,
                    work_area=previous.work_area,
                    physical_travel=telemetry.physical_travel or previous.physical_travel,
                    axes=telemetry.axes or previous.axes,
                    atc=telemetry.atc or previous.atc,
                    spindle=telemetry.spindle or previous.spindle,
                    tool_setter=previous.tool_setter,
                )
            self._snapshot = replace(self._snapshot, machine_state=merged)
            self._version += 1
            return self._snapshot

    def mark_offline(self) -> GuiStateSnapshot:
        with self._lock:
            self._snapshot = replace(
                self._snapshot,
                machine_online=False,
                power_transition=False,
                transport=None,
                machine_state=None,
            )
            self._version += 1
            return self._snapshot
