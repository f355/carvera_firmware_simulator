#!/usr/bin/env python3
"""Extract the LPC1768 memory layout from a real firmware ARM build."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any

_MEMORY_REGION_RE = re.compile(
    r"^\s*(?P<name>RAM|AHB_SRAM)\s+"
    r"(?P<origin>0x[0-9a-fA-F]+)\s+"
    r"(?P<length>0x[0-9a-fA-F]+)\b",
    re.MULTILINE,
)
_DIE_RE = re.compile(
    r"^\s*<\d+><[0-9a-fA-F]+>:\s+Abbrev Number:.*"
    r"\(DW_TAG_(?:class_type|structure_type|union_type)\)",
)
_ATTRIBUTE_RE = re.compile(r"DW_AT_(?P<name>name|byte_size)\s*:\s*(?P<value>.+?)\s*$")
_CONFIG_CACHE_CAPACITY_RE = re.compile(r"^\s*#define\s+CONFIG_CACHE_CAPACITY\s+(\d+)\b", re.MULTILINE)


def _parse_symbol(map_text: str, symbol: str) -> int:
    match = re.search(
        rf"^\s*(0x[0-9a-fA-F]+)\s+.*(?:\b|_){re.escape(symbol)}\b",
        map_text,
        re.MULTILINE,
    )
    if match is None:
        raise ValueError(f"linker map does not define {symbol}")
    return int(match.group(1), 16)


def _parse_memory_regions(map_text: str) -> dict[str, tuple[int, int]]:
    regions: dict[str, tuple[int, int]] = {}
    for match in _MEMORY_REGION_RE.finditer(map_text):
        origin = int(match.group("origin"), 16)
        length = int(match.group("length"), 16)
        regions[match.group("name")] = (origin, origin + length)

    missing = {"RAM", "AHB_SRAM"} - regions.keys()
    if missing:
        raise ValueError(f"linker map is missing memory region(s): {', '.join(sorted(missing))}")
    return regions


def _attribute_text(raw_value: str) -> str:
    indirect_separator = "): "
    if indirect_separator in raw_value:
        return raw_value.rsplit(indirect_separator, maxsplit=1)[1].strip()
    return raw_value.strip()


def parse_type_sizes(readelf_text: str, type_names: set[str] | None = None) -> dict[str, int]:
    """Return named ARM type sizes from a readelf DWARF info dump."""

    sizes: dict[str, int] = {}
    current_name: str | None = None
    current_size: int | None = None
    in_type = False

    def finish_type() -> None:
        if current_name is None or current_size is None:
            return
        if type_names is not None and current_name not in type_names:
            return
        previous = sizes.get(current_name)
        if previous is not None and previous != current_size:
            raise ValueError(f"conflicting ARM layouts for {current_name}: {previous} and {current_size} bytes")
        sizes[current_name] = current_size

    for line in [*readelf_text.splitlines(), "<0><0>: end"]:
        if re.match(r"^\s*<\d+><[0-9a-fA-F]+>:", line):
            finish_type()
            current_name = None
            current_size = None
            in_type = _DIE_RE.match(line) is not None
            continue
        if not in_type:
            continue

        attribute = _ATTRIBUTE_RE.search(line)
        if attribute is None:
            continue
        value = _attribute_text(attribute.group("value"))
        if attribute.group("name") == "name":
            current_name = value
        else:
            current_size = int(value, 0)

    return dict(sorted(sizes.items()))


def parse_config_cache_capacity(config_cache_header: str) -> int:
    match = _CONFIG_CACHE_CAPACITY_RE.search(config_cache_header)
    if match is None:
        raise ValueError("ConfigCache.h does not define CONFIG_CACHE_CAPACITY")
    return int(match.group(1))


def build_layout_report(
    map_text: str,
    readelf_text: str,
    *,
    firmware_commit: str,
    config_cache_capacity: int = 350,
    type_names: set[str] | None = None,
) -> dict[str, Any]:
    regions = _parse_memory_regions(map_text)
    type_sizes = parse_type_sizes(readelf_text, type_names)
    try:
        config_value_size = type_sizes["ConfigValue"]
    except KeyError as error:
        raise ValueError("ARM debug information does not contain ConfigValue") from error

    ram_start, ram_end = regions["RAM"]
    ahb_start, ahb_end = regions["AHB_SRAM"]
    static_end = _parse_symbol(map_text, "__end__")
    stack_top = _parse_symbol(map_text, "__StackTop")
    stack_limit = _parse_symbol(map_text, "__StackLimit")
    ahb_dynamic_start = _parse_symbol(map_text, "__AHB_dyn_start")
    heap_limit = (stack_limit - 32 + 31) & ~31

    if not (ram_start <= static_end <= heap_limit < stack_limit <= stack_top == ram_end):
        raise ValueError("main SRAM linker symbols are inconsistent")
    if not (ahb_start <= ahb_dynamic_start <= ahb_end):
        raise ValueError("AHB SRAM linker symbols are inconsistent")

    return {
        "schema_version": 1,
        "firmware_commit": firmware_commit,
        "main_sram": {
            "ram_start": ram_start,
            "ram_end": ram_end,
            "static_end": static_end,
            "stack_top": stack_top,
            "stack_limit": stack_limit,
            "heap_limit": heap_limit,
            "config_cache_bytes": config_cache_capacity * config_value_size,
        },
        "ahb_sram": {
            "region_start": ahb_start,
            "region_end": ahb_end,
            "dynamic_start": ahb_dynamic_start,
        },
        "type_sizes": type_sizes,
    }


def _find_readelf(firmware_root: Path) -> Path:
    bundled = sorted(firmware_root.glob("gcc-arm-none-eabi-*/bin/arm-none-eabi-readelf"))
    if bundled:
        return bundled[-1]
    executable = shutil.which("arm-none-eabi-readelf")
    if executable is None:
        raise RuntimeError("arm-none-eabi-readelf was not found; run with --build to fetch the firmware toolchain")
    return Path(executable)


def _run(command: list[str], *, cwd: Path) -> str:
    return subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout


def generate_report(firmware_root: Path, *, build: bool) -> dict[str, Any]:
    if build:
        subprocess.run([str(firmware_root / "build" / "build.sh"), "--release"], cwd=firmware_root, check=True)

    map_path = firmware_root / "LPC1768" / "main.map"
    elf_path = firmware_root / "LPC1768" / "main.elf"
    if not map_path.is_file() or not elf_path.is_file():
        raise RuntimeError("firmware ARM outputs are missing; run with --build")

    readelf_text = _run([str(_find_readelf(firmware_root)), "--debug-dump=info", str(elf_path)], cwd=firmware_root)
    firmware_commit = _run(["git", "rev-parse", "HEAD"], cwd=firmware_root).strip()
    config_cache_header = (firmware_root / "src" / "libs" / "ConfigCache.h").read_text()
    config_cache_capacity = parse_config_cache_capacity(config_cache_header)
    return build_layout_report(
        map_path.read_text(),
        readelf_text,
        firmware_commit=firmware_commit,
        config_cache_capacity=config_cache_capacity,
        type_names={
            "Block",
            "ATCHandler",
            "AD8495",
            "Adc",
            "AppendFileStream",
            "ConfigCache",
            "ConfigValue",
            "Config",
            "Configurator",
            "Conveyor",
            "CartesianSolution",
            "EEPROM_data",
            "FACTORY_SET",
            "Gcode",
            "GcodeDispatch",
            "Drillingcycles",
            "Endstops",
            "Kernel",
            "Laser",
            "MainButton",
            "Max31855",
            "PT100_E3D",
            "Pin",
            "Planner",
            "Player",
            "Pwm",
            "Robot",
            "SerialConsole",
            "SerialConsole2",
            "SimpleShell",
            "SlowTicker",
            "SoftPWM",
            "StepTicker",
            "StepperMotor",
            "StreamOutputPool",
            "Switch",
            "TemperatureControl",
            "TemperatureSwitch",
            "Thermistor",
            "Watchdog",
            "WifiProvider",
            "ZProbe",
        },
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build", action="store_true", help="build the firmware before extracting its layout")
    parser.add_argument("--check", action="store_true", help="verify that the output file is current")
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    report = generate_report(args.firmware_root.resolve(), build=args.build)
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.check:
        if not args.output.is_file() or args.output.read_text() != rendered:
            raise SystemExit(f"{args.output} does not match the current firmware ARM build")
        return 0
    args.output.write_text(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
