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

from gui.core.client_errors import should_notify_client_error


def test_app_runtime_error_test() -> None:
    if should_notify_client_error("simulator closed stdout", machine_online=False):
        raise SystemExit("expected shutdown should not show a red toast")
    if not should_notify_client_error("simulator closed stdout", machine_online=True):
        raise SystemExit("unexpected stdout closure while online should still be reported")
    if not should_notify_client_error("boom", machine_online=False):
        raise SystemExit("unrelated client errors should still be reported")
