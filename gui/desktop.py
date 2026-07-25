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

from multiprocessing import freeze_support

from nicegui import app

from gui.core.runtime_paths import resource_root
from gui.core.webview_runtime import (
    bundled_webview_settings,
    configure_bundled_webview_runtime,
    prepare_fixed_runtime,
)

RESOURCE_ROOT = resource_root()
BUNDLED_WEBVIEW2_SETTINGS = bundled_webview_settings(RESOURCE_ROOT)
BUNDLED_WEBVIEW2_RUNTIME = configure_bundled_webview_runtime(RESOURCE_ROOT)
if BUNDLED_WEBVIEW2_RUNTIME is not None:
    app.native.settings.update(BUNDLED_WEBVIEW2_SETTINGS)
    app.native.start_args["gui"] = "edgechromium"


if __name__ == "__main__":
    freeze_support()
    if BUNDLED_WEBVIEW2_RUNTIME is not None:
        prepare_fixed_runtime(BUNDLED_WEBVIEW2_RUNTIME)
    from gui.app_runtime import run

    run(native=True)
