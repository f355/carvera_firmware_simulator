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

from pathlib import Path

from gui.protocol.proto_codegen import add_generated_to_path
from gui.protocol.sim_client import SimulatorClient


add_generated_to_path()
import carvera_sim_pb2 as pb  # noqa: E402


class RecordingClient(SimulatorClient):
    def __init__(self) -> None:
        super().__init__(Path("/not/used"))
        self.requests: list[pb.Request] = []

    def request(self, request: pb.Request) -> pb.Response:
        self.requests.append(request)
        response = pb.Response()
        response.ok = True
        return response


def recorded_function_setting(model: str) -> int:
    client = RecordingClient()
    client.set_machine_model(model)
    return client.requests[-1].set_machine_model.function_setting


def test_sim_client_machine_model_test() -> None:
    assert recorded_function_setting("ca1") == 0x02
    assert recorded_function_setting("c1") == 0x02

    client = RecordingClient()
    client.set_rotary_accessory_installed(True)
    assert client.requests[-1].set_rotary_accessory_installed.installed is True
