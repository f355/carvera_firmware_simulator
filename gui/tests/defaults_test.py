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

from gui.core.defaults import (
    DEFAULT_C1_TOOLS,
    DEFAULT_LOOSE_TOOLS,
    DEFAULT_SD_CONFIG,
    PIN_WATCHES,
    PWM_WATCHES,
    SWITCH_WATCHES,
)


def test_default_sd_config_marks_card_present() -> None:
    assert "sd_ok true" in DEFAULT_SD_CONFIG


def test_default_tools_describe_a_loaded_c1_rack_and_locked_probes() -> None:
    assert len(DEFAULT_C1_TOOLS) == 7
    assert (DEFAULT_C1_TOOLS[0]["tool"], DEFAULT_C1_TOOLS[0]["locked"]) == (0, True)
    assert all(tool["occupied"] for tool in DEFAULT_C1_TOOLS)
    assert DEFAULT_LOOSE_TOOLS[0]["tool"] >= 999990
    assert DEFAULT_LOOSE_TOOLS[0]["locked"] is True


def test_default_signal_watches_use_valid_pins() -> None:
    for label, port, pin in PIN_WATCHES + PWM_WATCHES:
        assert label
        assert 0 <= port <= 4
        assert 0 <= pin <= 31
    assert "beep" in {name for name, _ in SWITCH_WATCHES}
