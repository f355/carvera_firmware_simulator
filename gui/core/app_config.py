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
import os
import sys
from collections.abc import Mapping
from pathlib import Path
from shutil import copy2

from .defaults import DEFAULT_SD_CONFIG
from .platform_paths import user_data_root


def default_stream_simulator(simulator_root: Path, *, platform: str | None = None) -> Path:
    platform = sys.platform if platform is None else platform
    executable_name = "carvera_sim_stream_stdio.exe" if platform == "win32" else "carvera_sim_stream_stdio"
    packaged_binary = simulator_root / "bin" / executable_name
    if packaged_binary.exists():
        return packaged_binary
    build_directory = "build-windows" if platform == "win32" else "build"
    return simulator_root / build_directory / executable_name


def default_sd_root() -> Path:
    return user_data_root() / "sdcard"


def default_sd_seed_root(simulator_root: Path) -> Path:
    return simulator_root / "default_sdcard"


def sd_root_for_model(sd_root: Path, machine_model: str) -> Path:
    return sd_root / machine_model


def sd_seed_root_for_model(seed_root: Path | None, machine_model: str) -> Path | None:
    if seed_root is None:
        return None
    model_seed_root = seed_root / machine_model
    return model_seed_root if model_seed_root.exists() else None


def default_firmware_root(simulator_root: Path, env: Mapping[str, str] | None = None) -> Path:
    env = os.environ if env is None else env
    configured_root = env.get("CARVERA_FIRMWARE_ROOT")
    if configured_root:
        return Path(configured_root)
    candidate = simulator_root.parent
    if (candidate / "src" / "config.default").exists():
        return candidate
    return simulator_root


def parse_vec3(text: str) -> tuple[float, float, float]:
    parts = [part.strip() for part in text.split(",")]
    if len(parts) != 3:
        raise ValueError(f"expected three comma-separated numbers, got {text!r}")
    return (float(parts[0]), float(parts[1]), float(parts[2]))


def sd_root_has_payload(sd_root: Path) -> bool:
    if not sd_root.exists():
        return False
    for path in sd_root.rglob("*"):
        if path.is_file() and not any(part.startswith(".") for part in path.relative_to(sd_root).parts):
            return True
    return False


def copy_sd_payload(source_root: Path, destination_root: Path, *, include_eeprom: bool = False) -> int:
    copied = 0
    for source in source_root.rglob("*"):
        relative = source.relative_to(source_root)
        if source.name == ".gitignore":
            continue
        if source.is_dir():
            (destination_root / relative).mkdir(parents=True, exist_ok=True)
            continue
        if not source.is_file():
            continue
        if source.name.startswith(".") and not (include_eeprom and source.name == ".eeprom.bin"):
            continue
        destination = destination_root / relative
        if destination.exists():
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        copy2(source, destination)
        copied += 1
    return copied


def prepare_sd_root(sd_root: Path, seed_root: Path | None = None) -> None:
    sd_root.mkdir(parents=True, exist_ok=True)
    if not sd_root_has_payload(sd_root) and seed_root is not None and seed_root.exists():
        if copy_sd_payload(seed_root, sd_root) > 0:
            return

    config_txt = sd_root / "config.txt"
    if not config_txt.exists() or config_txt.stat().st_size == 0:
        config_txt.write_text(DEFAULT_SD_CONFIG, encoding="utf-8")


def prepare_model_sd_root(sd_root: Path, seed_root: Path | None, machine_model: str) -> Path:
    model_sd_root = sd_root_for_model(sd_root, machine_model)
    model_sd_root.mkdir(parents=True, exist_ok=True)
    prepare_sd_root(model_sd_root, sd_seed_root_for_model(seed_root, machine_model))
    return model_sd_root


def build_arg_parser(simulator_root: Path, *, platform: str | None = None) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Carvera simulator GUI")
    parser.add_argument("--simulator", type=Path, default=default_stream_simulator(simulator_root, platform=platform))
    parser.add_argument("--firmware-root", type=Path, default=default_firmware_root(simulator_root))
    parser.add_argument("--sd-root", type=Path, default=default_sd_root())
    parser.add_argument("--model", choices=["c1", "ca1"], default="c1")
    parser.add_argument(
        "--machine-model",
        default=None,
        help="Optional GLB/GLTF/STL machine shell asset. Defaults to a bundled asset when available.",
    )
    parser.add_argument(
        "--machine-model-scale",
        type=float,
        default=1000.0,
        help="Scale for CAD shell assets. CAD Assistant GLB exports are usually meters, so 1000 maps them to mm.",
    )
    parser.add_argument(
        "--machine-model-offset",
        default=None,
        help="Machine shell offset in scene millimeters as x,y,z.",
    )
    parser.add_argument(
        "--machine-model-rotation",
        default=None,
        help="Machine shell Euler rotation in degrees as rx,ry,rz.",
    )
    parser.add_argument("--wifi-port", type=int, default=2222)
    parser.add_argument(
        "--no-log-transport",
        action="store_false",
        dest="log_transport",
        help="Do not mirror simulator UART/WiFi traffic to the terminal.",
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=None)
    parser.set_defaults(log_transport=True)
    return parser


def parse_args(simulator_root: Path) -> argparse.Namespace:
    return build_arg_parser(simulator_root).parse_args()
