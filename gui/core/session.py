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

import argparse
from dataclasses import dataclass
from math import radians
from pathlib import Path
from typing import Any, Self

from gui.core.app_config import default_sd_seed_root, parse_args, parse_vec3
from gui.core.gui_state import GuiStateStore
from gui.core.process_controller import SimulatorProcessController
from gui.core.telemetry import TelemetryBuffer
from gui.core.transport_log import TransportLogStore, parse_transport_log_line
from gui.protocol.model import (
    MachineState,
    PhysicalIoState,
    physical_io_to_state,
    snapshot_to_state,
    telemetry_to_state,
)
from gui.scene.backplot_layer import BackplotHistoryStore
from gui.protocol.sim_client import SimulatorClient
from gui.scene.model_registry import MachineModelRegistry


@dataclass(slots=True)
class SimulatorSession:
    simulator_root: Path
    args: argparse.Namespace
    machine_model_registry: MachineModelRegistry
    telemetry_buffer: TelemetryBuffer
    snapshot_buffer: TelemetryBuffer[MachineState]
    physical_io_buffer: TelemetryBuffer[PhysicalIoState]
    transport_log_store: TransportLogStore
    backplot_history: BackplotHistoryStore
    state_store: GuiStateStore
    client: SimulatorClient
    process_controller: SimulatorProcessController
    sd_root: Path
    sd_seed_root: Path
    machine_model_offset_override: tuple[float, float, float] | None
    machine_model_rotation_override: tuple[float, float, float] | None

    @classmethod
    def create(cls, simulator_root: Path, static_file_app: Any) -> Self:
        args = parse_args(simulator_root)
        offset_override = parse_vec3(args.machine_model_offset) if args.machine_model_offset is not None else None
        rotation_override: tuple[float, float, float] | None = None
        if args.machine_model_rotation is not None:
            rotate_x, rotate_y, rotate_z = parse_vec3(args.machine_model_rotation)
            rotation_override = (radians(rotate_x), radians(rotate_y), radians(rotate_z))

        machine_model_registry = MachineModelRegistry(
            simulator_root=simulator_root,
            static_file_app=static_file_app,
            configured_model=args.machine_model,
            offset_override=offset_override,
        )
        telemetry_buffer = TelemetryBuffer(telemetry_to_state)
        snapshot_buffer = TelemetryBuffer(snapshot_to_state, event_name="machine_snapshot")
        physical_io_buffer = TelemetryBuffer(physical_io_to_state, event_name="physical_io")
        transport_log_store = TransportLogStore()
        backplot_history = BackplotHistoryStore()
        state_store = GuiStateStore()

        def handle_stream_event(event: Any) -> None:
            telemetry_buffer.handle_stream_event(event)
            snapshot_buffer.handle_stream_event(event)
            physical_io_buffer.handle_stream_event(event)

        def handle_transport_stderr(line: str) -> None:
            entry = parse_transport_log_line(line)
            if entry is not None:
                transport_log_store.append(entry)

        client = SimulatorClient(
            args.simulator,
            stream_frames=True,
            event_handler=handle_stream_event,
            stderr_handler=handle_transport_stderr,
            inherit_stderr=args.log_transport,
        )
        sd_root = args.sd_root.expanduser()
        sd_seed_root = default_sd_seed_root(simulator_root)
        process_controller = SimulatorProcessController(
            client=client,
            sd_root=sd_root,
            sd_seed_root=sd_seed_root,
            wifi_port=args.wifi_port,
            log_transport=args.log_transport,
        )
        return cls(
            simulator_root=simulator_root,
            args=args,
            machine_model_registry=machine_model_registry,
            telemetry_buffer=telemetry_buffer,
            snapshot_buffer=snapshot_buffer,
            physical_io_buffer=physical_io_buffer,
            transport_log_store=transport_log_store,
            backplot_history=backplot_history,
            state_store=state_store,
            client=client,
            process_controller=process_controller,
            sd_root=sd_root,
            sd_seed_root=sd_seed_root,
            machine_model_offset_override=offset_override,
            machine_model_rotation_override=rotation_override,
        )
