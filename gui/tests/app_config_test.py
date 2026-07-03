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

import pytest

from gui.core.app_config import (
    build_arg_parser,
    default_firmware_root,
    default_sd_seed_root,
    parse_vec3,
    prepare_model_sd_root,
    prepare_sd_root,
    sd_root_for_model,
    sd_root_has_payload,
    sd_seed_root_for_model,
)


def write_seed_tree(root: Path) -> tuple[Path, Path, Path]:
    seed_root = root / "default_sdcard"
    seed_root.mkdir()
    (seed_root / "config.txt").write_text("sd_ok true\n", encoding="utf-8")
    (seed_root / "flex_compensation.dat").write_bytes(b"flex")
    (seed_root / "gcodes").mkdir()
    (seed_root / "gcodes" / "demo.nc").write_text("G0 X0\n", encoding="utf-8")

    c1_seed = seed_root / "c1"
    ca1_seed = seed_root / "ca1"
    c1_seed.mkdir()
    ca1_seed.mkdir()
    (c1_seed / "config.txt").write_text("# c1 config\n", encoding="utf-8")
    (ca1_seed / "config.txt").write_text("# ca1 config\n", encoding="utf-8")
    (ca1_seed / "flex_compensation.dat").write_bytes(b"ca1-flex")
    (ca1_seed / "gcodes").mkdir()
    return seed_root, c1_seed, ca1_seed


def test_parse_vec3_accepts_exactly_three_numbers() -> None:
    assert parse_vec3("1, 2.5, -3") == (1.0, 2.5, -3.0)
    with pytest.raises(ValueError):
        parse_vec3("1,2")


def test_default_firmware_root_honors_override_then_discovers_parent(tmp_path: Path) -> None:
    simulator_root = tmp_path / "carvera_simulator"
    firmware_root = tmp_path / "firmware"
    simulator_root.mkdir()

    assert default_firmware_root(simulator_root, env={"CARVERA_FIRMWARE_ROOT": str(firmware_root)}) == firmware_root
    assert default_firmware_root(simulator_root, env={}) == simulator_root

    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "config.default").write_text("# stock config\n", encoding="utf-8")
    assert default_firmware_root(simulator_root, env={}) == tmp_path


def test_model_sd_paths_are_isolated() -> None:
    root = Path("sdcard")
    assert sd_root_for_model(root, "c1") == root / "c1"
    assert sd_root_for_model(root, "ca1") == root / "ca1"


def test_prepare_sd_root_initializes_once(tmp_path: Path) -> None:
    sd_root = tmp_path / "sdcard"
    prepare_sd_root(sd_root)
    config = sd_root / "config.txt"
    assert "sd_ok true" in config.read_text(encoding="utf-8")

    config.write_text("# user config\n", encoding="utf-8")
    prepare_sd_root(sd_root)
    assert config.read_text(encoding="utf-8") == "# user config\n"


def test_parser_defaults_are_relative_to_simulator_root(tmp_path: Path) -> None:
    parser = build_arg_parser(tmp_path)
    args = parser.parse_args([])

    assert args.simulator == tmp_path / "build" / "carvera_sim_stream_stdio"
    assert args.sd_root == tmp_path / "sdcard"
    assert default_sd_seed_root(tmp_path) == tmp_path / "default_sdcard"
    assert args.wifi_port == 2222
    assert args.log_transport is True


def test_prepare_sd_root_copies_seed_payload(tmp_path: Path) -> None:
    seed_root, _, _ = write_seed_tree(tmp_path)
    sd_root = tmp_path / "seeded-sd"
    prepare_sd_root(sd_root, seed_root)

    assert (sd_root / "config.txt").read_text(encoding="utf-8") == "sd_ok true\n"
    assert (sd_root / "flex_compensation.dat").read_bytes() == b"flex"
    assert (sd_root / "gcodes" / "demo.nc").read_text(encoding="utf-8") == "G0 X0\n"


def test_prepare_model_sd_root_uses_only_its_model_seed(tmp_path: Path) -> None:
    seed_root, c1_seed, ca1_seed = write_seed_tree(tmp_path)
    assert sd_seed_root_for_model(seed_root, "c1") == c1_seed
    assert sd_seed_root_for_model(seed_root, "ca1") == ca1_seed

    split_root = tmp_path / "split-sd"
    c1_sd = prepare_model_sd_root(split_root, seed_root, "c1")
    ca1_sd = prepare_model_sd_root(split_root, seed_root, "ca1")

    assert (c1_sd / "config.txt").read_text(encoding="utf-8") == "# c1 config\n"
    assert (ca1_sd / "config.txt").read_text(encoding="utf-8") == "# ca1 config\n"
    assert not (c1_sd / "flex_compensation.dat").exists()
    assert (ca1_sd / "flex_compensation.dat").read_bytes() == b"ca1-flex"
    assert (ca1_sd / "gcodes").is_dir()


def test_hidden_files_do_not_make_an_sd_root_initialized(tmp_path: Path) -> None:
    seed_root, _, _ = write_seed_tree(tmp_path)
    sd_root = tmp_path / "ignored-only-sd"
    sd_root.mkdir()
    (sd_root / ".gitignore").write_text("*\n", encoding="utf-8")

    assert not sd_root_has_payload(sd_root)
    prepare_sd_root(sd_root, seed_root)
    assert (sd_root / "config.txt").exists()


def test_model_sd_roots_ignore_legacy_root_payload(tmp_path: Path) -> None:
    seed_root, _, _ = write_seed_tree(tmp_path)
    sd_root = tmp_path / "root-payload-sd"
    sd_root.mkdir()
    (sd_root / "config.txt").write_text("# legacy config\n", encoding="utf-8")
    (sd_root / ".eeprom.bin").write_bytes(b"legacy eeprom")

    ca1_sd = prepare_model_sd_root(sd_root, seed_root, "ca1")
    c1_sd = prepare_model_sd_root(sd_root, seed_root, "c1")

    assert ca1_sd == sd_root / "ca1"
    assert (ca1_sd / "config.txt").read_text(encoding="utf-8") == "# ca1 config\n"
    assert (c1_sd / "config.txt").read_text(encoding="utf-8") == "# c1 config\n"
    assert not (ca1_sd / ".eeprom.bin").exists()
    assert not (c1_sd / ".eeprom.bin").exists()
