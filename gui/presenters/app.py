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

from gui.core.session import SimulatorSession
from gui.presenters.appearance import AppearancePresenter
from gui.presenters.physical import PhysicalPresenter
from gui.presenters.power import PowerPresenter
from gui.presenters.service import ServicePresenter
from gui.presenters.state import StatePresenter
from gui.presenters.tooling import ToolingPresenter


class AppPresenters:
    def __init__(self, session: SimulatorSession) -> None:
        self.state = StatePresenter(session)
        self.tooling = ToolingPresenter(session)
        self.physical = PhysicalPresenter(session, self.state)
        self.service = ServicePresenter(session)
        self.appearance = AppearancePresenter(session)
        self.power = PowerPresenter(
            session,
            state=self.state,
            physical=self.physical,
            tooling=self.tooling,
            service=self.service,
        )
