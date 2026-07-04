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

from gui.generated import carvera_sim_pb2 as pb
from gui.protocol.model import EepromContents, PersistentVariable, WorkCoordinateSystem
from gui.protocol.sim_client import SimulatorClient


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


def test_client_maps_machine_models_and_rotary_accessory_to_protocol_requests() -> None:
    assert recorded_function_setting("ca1") == 0x02
    assert recorded_function_setting("c1") == 0x02

    client = RecordingClient()
    client.set_rotary_accessory_installed(True)
    assert client.requests[-1].set_rotary_accessory_installed.installed is True


def test_client_sends_structured_eeprom_contents() -> None:
    client = RecordingClient()
    contents = EepromContents(
        tool_length_offset=1.0,
        reference_machine_z=2.0,
        tool_machine_z=3.0,
        reserved=4.0,
        active_tool=5,
        tool_not_calibrated=True,
        current_wcs=6,
        persistent_variables=(PersistentVariable(number=501, value=7.0),),
        work_coordinate_systems=(WorkCoordinateSystem(number=54, x=8.0, y=9.0, z=10.0, a=11.0, rotation=12.0),),
    )

    client.set_eeprom_contents(contents)

    request_contents = client.requests[-1].set_eeprom_contents.contents
    assert request_contents.tool_length_offset == 1.0
    assert request_contents.persistent_variables[0].number == 501
    assert request_contents.work_coordinate_systems[0].rotation == 12.0
