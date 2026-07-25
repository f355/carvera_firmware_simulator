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

"""The cover switch is the one control driven by both the user and the IO stream.

FakeSwitch reproduces NiceGUI's dispatch semantics exactly (verified against
nicegui 3.12 binding.BindableProperty.__set__ and events.handle_event):

* assigning an unchanged value is a no-op that fires no handler, and
* assigning a changed value fires the on_change handler, which for an async
  handler is *scheduled as a background task* rather than awaited.

That second point is why a plain "I am updating controls" re-entrancy flag
cannot work: it is cleared synchronously, long before the scheduled handler
runs.
"""

from __future__ import annotations

import asyncio
from types import SimpleNamespace
from typing import Any, cast

from gui.core.gui_state import GuiStateStore
from gui.presenters.physical import PhysicalPresenter
from gui.presenters.state import StatePresenter
from gui.protocol.model import PhysicalIoState
from gui.tests.fakes import FakeLabel
from gui.views.io_panel import IoPanelView


class FakeSwitch:
    """A NiceGUI ValueElement stand-in with faithful change dispatch."""

    def __init__(self, value: bool, pending: list[Any]) -> None:
        self._value = value
        self._pending = pending
        self.on_change: Any = None

    @property
    def value(self) -> bool:
        return self._value

    @value.setter
    def value(self, new_value: bool) -> None:
        if self._value == new_value:
            return  # BindableProperty.__set__ short-circuits unchanged writes
        self._value = new_value
        if self.on_change is not None:
            self._pending.append(self.on_change(SimpleNamespace(value=new_value)))

    def click(self) -> None:
        """A user toggling the switch in the browser."""
        self.value = not self._value


def front_panel() -> Any:
    return SimpleNamespace(
        main_button_pressed=False,
        e_stop_pressed=False,
        power_rails=SimpleNamespace(v12=True, v24=True),
        direct_rgb_available=False,
        direct_rgb=SimpleNamespace(r=0, g=0, b=0),
        led_strip_available=False,
        led_strip=[],
    )


def io_snapshot(*, cover_open: bool) -> PhysicalIoState:
    return PhysicalIoState(
        probe_contact=False,
        tool_setter_contact=False,
        cover_open=cover_open,
        front_panel=front_panel(),
        motor_alarms={},
        spindle_alarm_available=False,
        spindle_alarm_triggered=False,
        switches={},
        laser=SimpleNamespace(available=False),
        pwm_outputs={},
    )


class CoverHarness:
    def __init__(self) -> None:
        self.commands: list[bool] = []
        self.pending: list[Any] = []
        self.switch = FakeSwitch(False, self.pending)

        store = GuiStateStore()
        store.set_online(transport=None)
        client = SimpleNamespace(set_cover_open=self.commands.append)
        controller = SimpleNamespace(call=self._call)
        session = SimpleNamespace(state_store=store, client=client, process_controller=controller)

        self.presenter = PhysicalPresenter(cast(Any, session), StatePresenter(cast(Any, session)))
        self.view = SimpleNamespace(io_panel_view=self._io_panel(), machine_tab_view=None)
        self.switch.on_change = lambda event: self.presenter.cover_changed(cast(Any, self.view), event)

    async def _call(self, method: Any, *args: Any, **kwargs: Any) -> Any:
        await asyncio.sleep(0)  # the real call hops threads; never resolves inline
        return method(*args, **kwargs)

    def _io_panel(self) -> IoPanelView:
        return IoPanelView(
            machine_model_value=lambda: "ca1",
            pwm_watches=(),
            probe_badge=FakeLabel(),
            tool_setter_badge=FakeLabel(),
            cover_switch=self.switch,
            cover_badge=FakeLabel(),
            temperature_status=FakeLabel(),
            front_panel_controls={},
            front_panel_badges={},
            motor_alarm_switches={},
            motor_alarm_badges={},
            spindle_alarm_switch=SimpleNamespace(value=False),
            spindle_alarm_badge=FakeLabel(),
            firmware_switch_controls={},
            laser_badges={},
            laser_power_label=FakeLabel(),
            pwm_labels={},
            ca1_led_strip_indicators=(),
        )

    async def settle(self) -> None:
        while self.pending:
            batch, self.pending[:] = list(self.pending), []
            await asyncio.gather(*batch)


def test_snapshot_driven_switch_write_does_not_echo_a_command() -> None:
    async def scenario() -> CoverHarness:
        harness = CoverHarness()
        # The simulator reports the cover open; the panel mirrors it into the switch.
        harness.view.io_panel_view.apply_snapshot(io_snapshot(cover_open=True))
        await harness.settle()
        return harness

    harness = asyncio.run(scenario())

    assert harness.switch.value is True, "the switch should mirror the reported cover state"
    assert harness.commands == [], "mirroring simulator state must not command the simulator"


def test_stale_snapshot_does_not_revert_a_user_opened_cover() -> None:
    async def scenario() -> CoverHarness:
        harness = CoverHarness()
        harness.switch.click()  # user opens the cover
        # The in-flight snapshot still carries the pre-click state.
        harness.view.io_panel_view.apply_snapshot(io_snapshot(cover_open=False))
        await harness.settle()
        return harness

    harness = asyncio.run(scenario())

    assert harness.commands == [True], f"user intent must survive a stale snapshot, got {harness.commands}"


def test_rapid_double_toggle_sends_both_user_commands() -> None:
    async def scenario() -> CoverHarness:
        harness = CoverHarness()
        harness.switch.click()  # open
        harness.switch.click()  # ...and immediately close again
        await harness.settle()
        return harness

    harness = asyncio.run(scenario())

    assert harness.commands == [True, False], "neither toggle may be swallowed as an echo"


def test_click_queued_before_a_mirroring_write_is_still_a_user_command() -> None:
    async def scenario() -> CoverHarness:
        harness = CoverHarness()
        harness.view.io_panel_view.apply_snapshot(io_snapshot(cover_open=True))
        await harness.settle()
        harness.commands.clear()

        # The user closes the cover, then a snapshot still reporting it open
        # lands before the click's handler has run.
        harness.switch.click()
        harness.view.io_panel_view.apply_snapshot(io_snapshot(cover_open=True))
        await harness.settle()
        return harness

    harness = asyncio.run(scenario())

    assert harness.commands == [False], f"the click must reach the simulator, got {harness.commands}"
