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

from gui.core.client_errors import is_request_timeout, should_disconnect_client_error, should_notify_client_error


def test_periodic_request_timeouts_are_not_fatal_client_errors() -> None:
    timeout = "simulator request 123 timed out after 10.0s"
    assert is_request_timeout(timeout)
    assert not should_notify_client_error(timeout, machine_online=True, periodic=True)
    assert not should_disconnect_client_error(timeout, machine_online=True, periodic=True)

    assert should_notify_client_error(timeout, machine_online=True, periodic=False)
    assert should_disconnect_client_error("short simulator response", machine_online=True, periodic=True)
