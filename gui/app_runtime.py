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

import signal
import sys
from pathlib import Path
from typing import Any

from nicegui import app, ui

SIMULATOR_ROOT = Path(__file__).resolve().parents[1]
if str(SIMULATOR_ROOT) not in sys.path:
    sys.path.insert(0, str(SIMULATOR_ROOT))

from gui.app_page import build_ui_page  # noqa: E402
from gui.core.session import SimulatorSession  # noqa: E402
from gui.presenters.app import AppPresenters  # noqa: E402


session = SimulatorSession.create(SIMULATOR_ROOT, app)
actions = AppPresenters(session)


@app.get("/healthz")
def healthz() -> dict[str, bool]:
    return {"ok": True}


app.on_shutdown(session.client.stop)


def run() -> None:
    def handle_shutdown_signal(signum: int, _frame: Any) -> None:
        print("\nShutting down Carvera simulator GUI...", file=sys.stderr)
        session.client.stop()
        app.shutdown()

    signal.signal(signal.SIGINT, handle_shutdown_signal)
    signal.signal(signal.SIGTERM, handle_shutdown_signal)
    try:
        ui.run(
            lambda: build_ui_page(session, actions),
            host=session.args.host,
            port=session.args.port,
            title="Carvera Simulator",
            reload=False,
            show=False,
        )
    except (KeyboardInterrupt, SystemExit):
        pass
    finally:
        session.client.stop()


if __name__ in {"__main__", "__mp_main__"}:
    run()
