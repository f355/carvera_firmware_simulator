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

import asyncio
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Protocol

from gui.core.app_config import prepare_model_sd_root
from gui.protocol.model import InteractiveTransportState, ToolConfig


class SimulatorProcessClient(Protocol):
    @property
    def powered(self) -> bool: ...

    def start(self) -> None: ...

    def stop(self) -> None: ...

    def set_machine_model(self, model: str) -> None: ...

    def mount_filesystem(self, name: str, root: Path) -> None: ...

    def set_rotary_accessory_installed(self, installed: bool) -> None: ...

    def set_atc_pocket_tools(self, tools: list[ToolConfig]) -> None: ...

    def set_realtime(self) -> None: ...

    def start_interactive_transport(
        self,
        *,
        enable_uart: bool = True,
        tcp_ports: list[int] | None = None,
        log_traffic: bool = False,
    ) -> InteractiveTransportState | None: ...

    def machine_snapshot(self) -> Any: ...


@dataclass(frozen=True, slots=True)
class PowerOnResult:
    transport: InteractiveTransportState | None
    snapshot: Any | None


@dataclass
class SimulatorProcessController:
    client: SimulatorProcessClient
    sd_root: Path
    sd_seed_root: Path
    wifi_port: int
    log_transport: bool
    current_sd_root: Path | None = None
    transport: InteractiveTransportState | None = None
    snapshot: Any | None = None
    _power_lock: asyncio.Lock = field(default_factory=asyncio.Lock)
    _call_lock: asyncio.Lock = field(default_factory=asyncio.Lock)

    async def call(self, method: Any, *method_args: Any, **method_kwargs: Any) -> Any:
        async with self._call_lock:
            return await asyncio.to_thread(method, *method_args, **method_kwargs)

    async def power_on(
        self, *, machine_model: str, tools: list[ToolConfig], rotary_enabled: bool = False
    ) -> PowerOnResult:
        async with self._power_lock:
            if self.client.powered:
                return PowerOnResult(transport=self.transport, snapshot=self.snapshot)

            self.transport = None
            self.snapshot = None
            try:
                self.current_sd_root = prepare_model_sd_root(self.sd_root, self.sd_seed_root, machine_model)
                await self.call(self.client.start)
                await self.call(self.client.set_machine_model, machine_model)
                await self.call(self.client.mount_filesystem, "sd", self.current_sd_root)
                await self.call(self.client.set_rotary_accessory_installed, rotary_enabled)
                await self.call(self.client.set_atc_pocket_tools, tools)
                await self.call(self.client.set_realtime)
                self.transport = await self.call(
                    self.client.start_interactive_transport,
                    enable_uart=True,
                    tcp_ports=[self.wifi_port],
                    log_traffic=self.log_transport,
                )
                return PowerOnResult(transport=self.transport, snapshot=self.snapshot)
            except Exception:
                self.transport = None
                self.snapshot = None
                await self.call(self.client.stop)
                raise

    async def power_off(self) -> None:
        async with self._power_lock:
            try:
                self.transport = None
                self.snapshot = None
                await self.call(self.client.stop)
            finally:
                self.current_sd_root = None

    async def stop_after_error(self) -> None:
        self.transport = None
        self.snapshot = None
        self.current_sd_root = None
        await self.call(self.client.stop)
