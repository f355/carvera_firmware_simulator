# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.

from __future__ import annotations

import json
from pathlib import Path

from gui.core.simulator_settings import (
    CameraPose,
    ModelMaterialSettings,
    SceneLightingSettings,
    SimulatorSettingsStore,
    StockSettings,
)


def test_settings_round_trip_global_and_per_machine_preferences(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    store = SimulatorSettingsStore(path)
    stock = StockSettings(True, -10.0, -20.0, -30.0, 10.0, 20.0, -5.0)
    camera = CameraPose(x=12.0, y=-34.0, z=56.0, zoom=1.75)
    lighting = SceneLightingSettings(exposure=1.2, shadows=False)
    material = ModelMaterialSettings(color="#123456", opacity=0.7)

    store.set_machine_model("ca1")
    store.set_show_3d_machine(False)
    store.set_realtime_speed(3.5)
    store.set_splitter_position(58.0)
    store.set_comms_autoscroll(False)
    store.set_rotary_accessory("ca1", True)
    store.set_stock("ca1", stock)
    store.set_camera("ca1", camera)
    store.set_appearance(lighting, material)

    restored = SimulatorSettingsStore(path).snapshot()
    assert restored.machine_model == "ca1"
    assert restored.show_3d_machine is False
    assert restored.realtime_speed == 3.5
    assert restored.splitter_position == 58.0
    assert restored.comms_autoscroll is False
    assert restored.machines["ca1"].rotary_accessory is True
    assert restored.machines["ca1"].stock == stock
    assert restored.machines["ca1"].camera == camera
    assert restored.lighting == lighting
    assert restored.material == material


def test_settings_are_isolated_per_machine(tmp_path: Path) -> None:
    store = SimulatorSettingsStore(tmp_path / "settings.json")
    c1_stock = StockSettings(True, 1, 2, 3, 4, 5, 6)
    ca1_stock = StockSettings(False, 7, 8, 9, 10, 11, 12)

    store.set_stock("c1", c1_stock)
    store.set_stock("ca1", ca1_stock)
    store.set_camera("z1", CameraPose(x=99))

    settings = store.snapshot()
    assert settings.machines["c1"].stock == c1_stock
    assert settings.machines["ca1"].stock == ca1_stock
    assert settings.machines["z1"].camera == CameraPose(x=99)


def test_invalid_or_outdated_settings_fall_back_to_defaults(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text("not json", encoding="utf-8")
    assert SimulatorSettingsStore(path).snapshot().machine_model == "c1"

    path.write_text(json.dumps({"version": 999, "machine_model": "z1"}), encoding="utf-8")
    assert SimulatorSettingsStore(path).snapshot().machine_model == "c1"


def test_unknown_models_are_not_loaded_or_saved(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text(
        json.dumps(
            {
                "version": 1,
                "machine_model": "not-a-machine",
                "machines": {"not-a-machine": {"rotary_accessory": True}},
            }
        ),
        encoding="utf-8",
    )
    store = SimulatorSettingsStore(path)

    store.set_machine_model("also-not-a-machine")

    settings = store.snapshot()
    assert settings.machine_model == "c1"
    assert settings.machines == {}


def test_settings_write_is_atomic_and_leaves_no_temporary_file(tmp_path: Path) -> None:
    path = tmp_path / "nested" / "settings.json"
    store = SimulatorSettingsStore(path)

    store.set_machine_model("z1pro")

    assert json.loads(path.read_text(encoding="utf-8"))["machine_model"] == "z1pro"
    assert not path.with_name(".settings.json.tmp").exists()
