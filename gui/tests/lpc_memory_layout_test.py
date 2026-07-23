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

from scripts.lpc_memory_layout import build_layout_report, parse_type_sizes


MAP_TEXT = """
Memory Configuration

Name             Origin             Length             Attributes
FLASH            0x00004000         0x0007c000         xr
RAM              0x100000c8         0x00007f38         xrw
AHB_SRAM         0x2007c000         0x00008000         xrw

                0x10000fd0                        __end__ = .
                0x10008000                        __StackTop = (ORIGIN (RAM) + LENGTH (RAM))
                0x10007000                        __StackLimit = (__StackTop - SIZEOF (.stack_dummy))
.AHBSRAM        0x2007c000     0x3ca8
                0x2007c000                        PROVIDE (__AHB_block_start = .)
                0x2007fca8                        PROVIDE (__AHB_dyn_start = .)
                0x20084000                        PROVIDE (__AHB_end = (ORIGIN (AHB_SRAM) + LENGTH (AHB_SRAM)))
"""

READELF_TEXT = """
 <1><fe47a>: Abbrev Number: 38 (DW_TAG_class_type)
    <fe47b>   DW_AT_name        : (indirect string, offset: 0x3b23d): ConfigValue
    <fe47f>   DW_AT_byte_size   : 26
 <1><fe6d1>: Abbrev Number: 0
 <1><feed0>: Abbrev Number: 38 (DW_TAG_class_type)
    <feed1>   DW_AT_name        : Block
    <feed2>   DW_AT_byte_size   : 160
"""


def test_build_layout_report_uses_linker_addresses_and_arm_type_sizes() -> None:
    report = build_layout_report(
        MAP_TEXT,
        READELF_TEXT,
        firmware_commit="0123456789abcdef",
    )

    assert report == {
        "schema_version": 1,
        "firmware_commit": "0123456789abcdef",
        "main_sram": {
            "ram_start": 0x100000C8,
            "ram_end": 0x10008000,
            "static_end": 0x10000FD0,
            "stack_top": 0x10008000,
            "stack_limit": 0x10007000,
            "heap_limit": 0x10006FE0,
            "config_cache_bytes": 9100,
        },
        "ahb_sram": {
            "region_start": 0x2007C000,
            "region_end": 0x20084000,
            "dynamic_start": 0x2007FCA8,
        },
        "type_sizes": {
            "Block": 160,
            "ConfigValue": 26,
        },
    }


def test_parse_type_sizes_rejects_conflicting_arm_layouts() -> None:
    conflicting = (
        READELF_TEXT
        + """
 <1><fff00>: Abbrev Number: 38 (DW_TAG_class_type)
    <fff01>   DW_AT_name        : ConfigValue
    <fff02>   DW_AT_byte_size   : 28
"""
    )

    try:
        parse_type_sizes(conflicting)
    except ValueError as error:
        assert "ConfigValue" in str(error)
    else:
        raise AssertionError("conflicting target sizes should not be silently accepted")
