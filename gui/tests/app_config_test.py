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
from tempfile import TemporaryDirectory

from gui.core.app_config import (
    build_arg_parser,
    default_firmware_root,
    default_sd_seed_root,
    parse_vec3,
    prepare_model_sd_root,
    prepare_sd_root,
    sd_root_for_model,
    sd_seed_root_for_model,
    sd_root_has_payload,
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def test_app_config_test() -> None:
    require(parse_vec3("1, 2.5, -3") == (1.0, 2.5, -3.0), "parse_vec3 should parse comma-separated floats")
    try:
        parse_vec3("1,2")
    except ValueError:
        pass
    else:
        raise SystemExit("parse_vec3 should reject malformed input")

    with TemporaryDirectory() as temp:
        root = Path(temp)
        simulator_root = root / "carvera_simulator"
        firmware_root = root / "firmware"
        simulator_root.mkdir()

        require(
            default_firmware_root(simulator_root, env={"CARVERA_FIRMWARE_ROOT": str(firmware_root)}) == firmware_root,
            "CARVERA_FIRMWARE_ROOT should override firmware-root discovery",
        )
        require(
            default_firmware_root(simulator_root, env={}) == simulator_root,
            "simulator root should be the fallback firmware root",
        )
        (root / "src").mkdir()
        (root / "src" / "config.default").write_text("# stock config\n", encoding="utf-8")
        require(
            default_firmware_root(simulator_root, env={}) == root,
            "parent firmware checkout should be discovered when stock config exists",
        )

        sd_root = root / "sdcard"
        require(sd_root_for_model(sd_root, "c1") == sd_root / "c1", "C1 SD state should live under sdcard/c1")
        require(sd_root_for_model(sd_root, "ca1") == sd_root / "ca1", "CA1 SD state should live under sdcard/ca1")
        prepare_sd_root(sd_root)
        config_txt = sd_root / "config.txt"
        require(config_txt.exists(), "prepare_sd_root should create config.txt")
        require("sd_ok true" in config_txt.read_text(encoding="utf-8"), "fresh SD config should mark the card present")
        config_txt.write_text("# user config\n", encoding="utf-8")
        prepare_sd_root(sd_root)
        require(
            config_txt.read_text(encoding="utf-8") == "# user config\n", "prepare_sd_root should not overwrite config"
        )

        parser = build_arg_parser(simulator_root)
        args = parser.parse_args([])
        require(args.simulator == simulator_root / "build" / "carvera_sim_stream_stdio", "unexpected simulator default")
        require(args.sd_root == simulator_root / "sdcard", "unexpected SD root default")
        require(default_sd_seed_root(simulator_root) == simulator_root / "default_sdcard", "unexpected SD seed default")
        require(args.wifi_port == 2222, "unexpected WiFi port default")
        require(args.log_transport is True, "transport logging should default on")

        seeded_sd = root / "seeded-sd"
        seed_root = root / "default_sdcard"
        seed_root.mkdir()
        (seed_root / "config.txt").write_text("sd_ok true\n", encoding="utf-8")
        (seed_root / "flex_compensation.dat").write_bytes(b"flex")
        (seed_root / "gcodes").mkdir()
        (seed_root / "gcodes" / ".gitkeep").write_text("", encoding="utf-8")
        (seed_root / "gcodes" / "demo.nc").write_text("G0 X0\n", encoding="utf-8")
        prepare_sd_root(seeded_sd, seed_root)
        require((seeded_sd / "config.txt").read_text(encoding="utf-8") == "sd_ok true\n", "seed config not copied")
        require((seeded_sd / "flex_compensation.dat").read_bytes() == b"flex", "seed flex data not copied")
        require((seeded_sd / "gcodes").is_dir(), "seed gcodes directory not copied")
        require((seeded_sd / "gcodes" / "demo.nc").read_text(encoding="utf-8") == "G0 X0\n", "seed gcode not copied")

        c1_seed_root = seed_root / "c1"
        ca1_seed_root = seed_root / "ca1"
        c1_seed_root.mkdir()
        ca1_seed_root.mkdir()
        (c1_seed_root / "config.txt").write_text("# c1 config\n", encoding="utf-8")
        (ca1_seed_root / "config.txt").write_text("# ca1 config\n", encoding="utf-8")
        (ca1_seed_root / "flex_compensation.dat").write_bytes(b"ca1-flex")
        (ca1_seed_root / "gcodes").mkdir()
        (ca1_seed_root / "gcodes" / ".gitkeep").write_text("", encoding="utf-8")
        require(sd_seed_root_for_model(seed_root, "c1") == c1_seed_root, "C1 should use a C1-specific SD seed")
        require(sd_seed_root_for_model(seed_root, "ca1") == ca1_seed_root, "CA1 should use a CA1-specific SD seed")

        split_sd = root / "split-sd"
        c1_split_sd = prepare_model_sd_root(split_sd, seed_root, "c1")
        ca1_split_sd = prepare_model_sd_root(split_sd, seed_root, "ca1")
        require(
            (c1_split_sd / "config.txt").read_text(encoding="utf-8") == "# c1 config\n",
            "C1 should receive only the C1 default config",
        )
        require(
            (ca1_split_sd / "config.txt").read_text(encoding="utf-8") == "# ca1 config\n",
            "CA1 should receive only the CA1 default config",
        )
        require(not (c1_split_sd / "flex_compensation.dat").exists(), "C1 should not inherit CA1 flex compensation")
        require((ca1_split_sd / "flex_compensation.dat").read_bytes() == b"ca1-flex", "CA1 flex data not copied")
        require((ca1_split_sd / "gcodes").is_dir(), "CA1 should receive its gcodes directory")

        ignored_only_sd = root / "ignored-only-sd"
        ignored_only_sd.mkdir()
        (ignored_only_sd / ".gitignore").write_text("*\n", encoding="utf-8")
        require(not sd_root_has_payload(ignored_only_sd), "hidden SD files should not count as payload")
        prepare_sd_root(ignored_only_sd, seed_root)
        require((ignored_only_sd / "config.txt").exists(), "ignored-only SD root should receive seed files")

        root_payload_sd = root / "root-payload-sd"
        root_payload_sd.mkdir()
        (root_payload_sd / "config.txt").write_text("# root payload should stay ignored\n", encoding="utf-8")
        (root_payload_sd / ".eeprom.bin").write_bytes(b"root eeprom")
        ca1_sd = prepare_model_sd_root(root_payload_sd, seed_root, "ca1")
        c1_sd = prepare_model_sd_root(root_payload_sd, seed_root, "c1")
        require(ca1_sd == root_payload_sd / "ca1", "prepare_model_sd_root should return the model SD root")
        require(
            (ca1_sd / "config.txt").read_text(encoding="utf-8") == "# ca1 config\n",
            "CA1 should ignore root-level SD payload and use its model seed",
        )
        require(
            (c1_sd / "config.txt").read_text(encoding="utf-8") == "# c1 config\n",
            "C1 should ignore root-level SD payload and use its model seed",
        )
        require(not (ca1_sd / ".eeprom.bin").exists(), "CA1 should not inherit root-level EEPROM payload")
        require(not (c1_sd / ".eeprom.bin").exists(), "C1 should not inherit root-level EEPROM payload")
