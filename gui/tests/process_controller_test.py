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
from pathlib import Path
from tempfile import TemporaryDirectory

from gui.core.process_controller import SimulatorProcessController
from gui.protocol.model import InteractiveTransportState, ToolConfig, ToolKind


class FakeClient:
    def __init__(self) -> None:
        self.calls: list[tuple[str, tuple, dict]] = []
        self.stopped = False
        self.started = False

    @property
    def powered(self) -> bool:
        return self.started and not self.stopped

    def start(self) -> None:
        self.started = True
        self.stopped = False
        self.calls.append(("start", (), {}))

    def set_machine_model(self, model: str) -> None:
        self.calls.append(("set_machine_model", (model,), {}))

    def mount_filesystem(self, name: str, root: Path) -> None:
        self.calls.append(("mount_filesystem", (name, root), {}))

    def set_rotary_accessory_installed(self, installed: bool) -> None:
        self.calls.append(("set_rotary_accessory_installed", (installed,), {}))

    def set_atc_pocket_tools(self, tools: list[ToolConfig]) -> None:
        self.calls.append(("set_atc_pocket_tools", (tools,), {}))

    def set_realtime(self) -> None:
        self.calls.append(("set_realtime", (), {}))

    def machine_snapshot(self) -> object:
        self.calls.append(("machine_snapshot", (), {}))
        return object()

    def start_interactive_transport(self, **kwargs):
        self.calls.append(("start_interactive_transport", (), kwargs))
        return InteractiveTransportState(uart_supported=True, uart_path="/dev/pty", tcp_endpoints=())

    def stop(self) -> None:
        self.stopped = True
        self.calls.append(("stop", (), {}))


class SlowFakeClient(FakeClient):
    def set_machine_model(self, model: str) -> None:
        import time

        time.sleep(0.05)
        super().set_machine_model(model)


async def exercise_process_controller() -> None:
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        sd_root = root / "sd"
        seed_root = root / "seed"
        ca1_seed_root = seed_root / "ca1"
        ca1_seed_root.mkdir(parents=True)
        (ca1_seed_root / "config.txt").write_text("sd_ok true\n", encoding="utf-8")
        client = FakeClient()
        process_controller = SimulatorProcessController(
            client=client,
            sd_root=sd_root,
            sd_seed_root=seed_root,
            wifi_port=2222,
            log_transport=True,
        )
        tools = [
            ToolConfig(
                pocket=1,
                tool=1,
                length_mm=40.0,
                occupied=True,
                kind=ToolKind.CUTTING_TOOL,
            )
        ]
        result = await process_controller.power_on(machine_model="ca1", tools=tools, rotary_enabled=True)
        mounted_sd_root = sd_root / "ca1"
        assert result.transport == InteractiveTransportState(
            uart_supported=True, uart_path="/dev/pty", tcp_endpoints=()
        )
        assert result.snapshot is None
        assert process_controller.transport == result.transport
        assert not hasattr(process_controller, "machine_online")
        assert not hasattr(process_controller, "power_transition")
        assert (mounted_sd_root / "config.txt").read_text(encoding="utf-8") == "sd_ok true\n"
        assert [name for name, _, _ in client.calls] == [
            "start",
            "set_machine_model",
            "mount_filesystem",
            "set_rotary_accessory_installed",
            "set_atc_pocket_tools",
            "set_realtime",
            "start_interactive_transport",
        ]
        assert client.calls[3][1] == (True,)
        assert client.calls[2][1] == ("sd", mounted_sd_root)
        assert client.calls[-1][2] == {"enable_uart": True, "tcp_ports": [2222], "log_traffic": True}
        assert process_controller.current_sd_root == mounted_sd_root

        await process_controller.power_off()
        assert client.powered is False
        assert client.stopped is True


async def exercise_concurrent_power_on() -> None:
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        sd_root = root / "sd"
        seed_root = root / "seed"
        c1_seed_root = seed_root / "c1"
        c1_seed_root.mkdir(parents=True)
        (c1_seed_root / "config.txt").write_text("sd_ok true\n", encoding="utf-8")
        client = SlowFakeClient()
        process_controller = SimulatorProcessController(
            client=client,
            sd_root=sd_root,
            sd_seed_root=seed_root,
            wifi_port=2222,
            log_transport=True,
        )

        first, second = await asyncio.gather(
            process_controller.power_on(machine_model="c1", tools=[], rotary_enabled=False),
            process_controller.power_on(machine_model="c1", tools=[], rotary_enabled=False),
        )

        assert first.transport == InteractiveTransportState(uart_supported=True, uart_path="/dev/pty", tcp_endpoints=())
        assert second.transport == InteractiveTransportState(
            uart_supported=True, uart_path="/dev/pty", tcp_endpoints=()
        )
        assert [name for name, _, _ in client.calls].count("set_machine_model") == 1
        assert [name for name, _, _ in client.calls].count("machine_snapshot") == 0
        assert [name for name, _, _ in client.calls].count("start_interactive_transport") == 1


def test_process_controller_power_sequence() -> None:
    asyncio.run(exercise_process_controller())


def test_process_controller_serializes_concurrent_power_on() -> None:
    asyncio.run(exercise_concurrent_power_on())
