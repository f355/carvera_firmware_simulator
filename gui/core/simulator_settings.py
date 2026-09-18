# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.

from __future__ import annotations

import json
import os
from copy import deepcopy
from dataclasses import asdict, dataclass, field
from pathlib import Path
from threading import RLock
from typing import Any, Final, Self

SUPPORTED_MACHINE_MODELS: Final = frozenset({"c1", "ca1", "z1", "z1pro"})
SETTINGS_VERSION: Final = 1
DEFAULT_MODEL_COLOR: Final = "#d9dee4"


def _finite_float(value: Any, default: float) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return default
    return result if result == result and abs(result) != float("inf") else default


@dataclass(frozen=True, slots=True)
class StockSettings:
    enabled: bool
    min_x: float
    min_y: float
    min_z: float
    max_x: float
    max_y: float
    max_z: float

    @classmethod
    def from_dict(cls, value: Any) -> Self | None:
        if not isinstance(value, dict):
            return None
        names = ("min_x", "min_y", "min_z", "max_x", "max_y", "max_z")
        if any(not isinstance(value.get(name), (int, float)) for name in names):
            return None
        return cls(
            enabled=bool(value.get("enabled", False)),
            **{name: _finite_float(value[name], 0.0) for name in names},
        )


@dataclass(frozen=True, slots=True)
class CameraPose:
    x: float = 430.0
    y: float = -560.0
    z: float = 320.0
    look_at_x: float = 0.0
    look_at_y: float = 0.0
    look_at_z: float = 70.0
    up_x: float = 0.0
    up_y: float = 0.0
    up_z: float = 1.0
    zoom: float = 1.0

    @classmethod
    def from_dict(cls, value: Any) -> Self | None:
        if not isinstance(value, dict):
            return None
        defaults = cls()
        values = {name: _finite_float(value.get(name), getattr(defaults, name)) for name in cls.__dataclass_fields__}
        values["zoom"] = max(0.05, min(20.0, values["zoom"]))
        return cls(**values)


@dataclass(frozen=True, slots=True)
class SceneLightingSettings:
    ambient: float = 0.2
    key: float = 0.5
    key_x: float = -50.0
    key_y: float = -250.0
    key_z: float = 250.0
    fill: float = 0.3
    fill_x: float = 40.0
    fill_y: float = 170.0
    fill_z: float = 160.0
    exposure: float = 0.88
    shadows: bool = True
    shadow_radius: float = 3.0
    shadow_bias: float = -0.003
    shadow_map_size: float = 1536.0

    @classmethod
    def from_dict(cls, value: Any) -> Self:
        if not isinstance(value, dict):
            return cls()
        defaults = cls()
        return cls(
            ambient=_finite_float(value.get("ambient"), defaults.ambient),
            key=_finite_float(value.get("key"), defaults.key),
            key_x=_finite_float(value.get("key_x"), defaults.key_x),
            key_y=_finite_float(value.get("key_y"), defaults.key_y),
            key_z=_finite_float(value.get("key_z"), defaults.key_z),
            fill=_finite_float(value.get("fill"), defaults.fill),
            fill_x=_finite_float(value.get("fill_x"), defaults.fill_x),
            fill_y=_finite_float(value.get("fill_y"), defaults.fill_y),
            fill_z=_finite_float(value.get("fill_z"), defaults.fill_z),
            exposure=_finite_float(value.get("exposure"), defaults.exposure),
            shadows=bool(value.get("shadows", defaults.shadows)),
            shadow_radius=_finite_float(value.get("shadow_radius"), defaults.shadow_radius),
            shadow_bias=_finite_float(value.get("shadow_bias"), defaults.shadow_bias),
            shadow_map_size=_finite_float(value.get("shadow_map_size"), defaults.shadow_map_size),
        )


@dataclass(frozen=True, slots=True)
class ModelMaterialSettings:
    color: str = DEFAULT_MODEL_COLOR
    opacity: float = 1.0
    roughness: float = 0.45
    metalness: float = 0.1

    @classmethod
    def from_dict(cls, value: Any) -> Self:
        if not isinstance(value, dict):
            return cls()
        defaults = cls()
        color = value.get("color", defaults.color)
        return cls(
            color=color if isinstance(color, str) else defaults.color,
            opacity=_finite_float(value.get("opacity"), defaults.opacity),
            roughness=_finite_float(value.get("roughness"), defaults.roughness),
            metalness=_finite_float(value.get("metalness"), defaults.metalness),
        )


@dataclass(slots=True)
class MachineSettings:
    rotary_accessory: bool = False
    stock: StockSettings | None = None
    camera: CameraPose | None = None

    @classmethod
    def from_dict(cls, value: Any) -> Self:
        if not isinstance(value, dict):
            return cls()
        return cls(
            rotary_accessory=bool(value.get("rotary_accessory", False)),
            stock=StockSettings.from_dict(value.get("stock")),
            camera=CameraPose.from_dict(value.get("camera")),
        )


@dataclass(slots=True)
class SimulatorSettings:
    machine_model: str = "c1"
    show_3d_machine: bool = True
    realtime_speed: float = 1.0
    splitter_position: float = 64.0
    comms_autoscroll: bool = True
    lighting: SceneLightingSettings = field(default_factory=SceneLightingSettings)
    material: ModelMaterialSettings = field(default_factory=ModelMaterialSettings)
    machines: dict[str, MachineSettings] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, value: Any) -> Self:
        if not isinstance(value, dict) or value.get("version") != SETTINGS_VERSION:
            return cls()
        machine_model = value.get("machine_model")
        machines_value = value.get("machines")
        machines = (
            {
                model: MachineSettings.from_dict(machine_settings)
                for model, machine_settings in machines_value.items()
                if model in SUPPORTED_MACHINE_MODELS
            }
            if isinstance(machines_value, dict)
            else {}
        )
        return cls(
            machine_model=machine_model if machine_model in SUPPORTED_MACHINE_MODELS else "c1",
            show_3d_machine=bool(value.get("show_3d_machine", True)),
            realtime_speed=max(0.25, min(10.0, _finite_float(value.get("realtime_speed"), 1.0))),
            splitter_position=max(20.0, min(80.0, _finite_float(value.get("splitter_position"), 64.0))),
            comms_autoscroll=bool(value.get("comms_autoscroll", True)),
            lighting=SceneLightingSettings.from_dict(value.get("lighting")),
            material=ModelMaterialSettings.from_dict(value.get("material")),
            machines=machines,
        )

    def to_dict(self) -> dict[str, Any]:
        return {"version": SETTINGS_VERSION, **asdict(self)}


class SimulatorSettingsStore:
    """Thread-safe, atomic storage for simulator-owned UI preferences."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self._lock = RLock()
        self._settings = self._load()

    def _load(self) -> SimulatorSettings:
        try:
            return SimulatorSettings.from_dict(json.loads(self.path.read_text(encoding="utf-8")))
        except (OSError, UnicodeError, json.JSONDecodeError):
            return SimulatorSettings()

    def snapshot(self) -> SimulatorSettings:
        with self._lock:
            return deepcopy(self._settings)

    def set_machine_model(self, model: str) -> None:
        if model not in SUPPORTED_MACHINE_MODELS:
            return
        with self._lock:
            self._settings.machine_model = model
            self._save()

    def set_show_3d_machine(self, visible: bool) -> None:
        with self._lock:
            self._settings.show_3d_machine = visible
            self._save()

    def set_realtime_speed(self, multiplier: float) -> None:
        with self._lock:
            self._settings.realtime_speed = max(0.25, min(10.0, multiplier))
            self._save()

    def set_splitter_position(self, position: float) -> None:
        with self._lock:
            self._settings.splitter_position = max(20.0, min(80.0, position))
            self._save()

    def set_comms_autoscroll(self, enabled: bool) -> None:
        with self._lock:
            self._settings.comms_autoscroll = enabled
            self._save()

    def set_rotary_accessory(self, model: str, installed: bool) -> None:
        with self._lock:
            self._machine(model).rotary_accessory = installed
            self._save()

    def set_stock(self, model: str, stock: StockSettings) -> None:
        with self._lock:
            self._machine(model).stock = stock
            self._save()

    def set_camera(self, model: str, camera: CameraPose) -> None:
        with self._lock:
            machine = self._machine(model)
            if machine.camera == camera:
                return
            machine.camera = camera
            self._save()

    def set_appearance(self, lighting: SceneLightingSettings, material: ModelMaterialSettings) -> None:
        with self._lock:
            self._settings.lighting = lighting
            self._settings.material = material
            self._save()

    def _machine(self, model: str) -> MachineSettings:
        if model not in SUPPORTED_MACHINE_MODELS:
            raise ValueError(f"unsupported machine model: {model}")
        return self._settings.machines.setdefault(model, MachineSettings())

    def _save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_name(f".{self.path.name}.tmp")
        temporary.write_text(
            json.dumps(self._settings.to_dict(), indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, self.path)
