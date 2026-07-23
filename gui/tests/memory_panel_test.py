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

from gui.protocol.model import (
    AhbSramMemory,
    MainSramMemory,
    MemoryAllocationGroup,
    MemoryDetails,
    MemoryRegion,
    MemorySummary,
)
from gui.tests.fakes import FakeControl, FakeLabel
from gui.views.memory_panel import MemoryPanelView


def memory_summary() -> MemorySummary:
    return MemorySummary(
        main=MainSramMemory(
            capacity_bytes=32_568,
            static_bytes=8_000,
            stack_reserved_bytes=2_048,
            heap_committed_bytes=12_000,
            live_payload_bytes=5_120,
            peak_live_payload_bytes=6_144,
            allocator_overhead_bytes=128,
            fragmented_free_bytes=512,
            largest_free_block_bytes=10_240,
            top_unallocated_bytes=15_360,
            minimum_margin_bytes=9_216,
            config_cache_active=True,
            config_cache_start=0x10006000,
            config_cache_bytes=2_048,
            config_cache_collision=False,
            failed_allocation_count=0,
            failed_allocation_bytes=0,
            heap_limit_collision=False,
            total_free_bytes=18_432,
        ),
        ahb=AhbSramMemory(
            capacity_bytes=32_768,
            static_bytes=8_192,
            dynamic_capacity_bytes=24_576,
            live_payload_bytes=12_288,
            peak_live_payload_bytes=14_336,
            allocator_overhead_bytes=256,
            total_free_bytes=4_096,
            largest_free_block_bytes=3_072,
            failed_allocation_count=0,
            failed_allocation_bytes=0,
        ),
        unresolved_main_live_host_bytes=0,
        unresolved_main_peak_host_bytes=0,
        unresolved_ahb_live_host_bytes=0,
        unresolved_ahb_peak_host_bytes=0,
    )


def memory_panel() -> MemoryPanelView:
    copy_details_button = FakeControl()
    copy_details_button.disable()
    return MemoryPanelView(
        main_live_label=FakeLabel(),
        main_peak_label=FakeLabel(),
        main_free_label=FakeLabel(),
        main_margin_label=FakeLabel(),
        ahb_live_label=FakeLabel(),
        ahb_peak_label=FakeLabel(),
        ahb_free_label=FakeLabel(),
        ahb_largest_label=FakeLabel(),
        status_label=FakeLabel(),
        details_label=FakeLabel(),
        copy_details_button=copy_details_button,
    )


def test_memory_panel_updates_periodic_summary_and_resets() -> None:
    panel = memory_panel()

    panel.update_summary(memory_summary())

    assert panel.main_live_label.text == "5,120 / 32,568 B"
    assert panel.main_margin_label.text == "9,216 B"
    assert panel.ahb_live_label.text == "20,736 / 32,768 B"
    assert panel.ahb_largest_label.text == "3,072 B"
    assert panel.status_label.text == "Config cache 2,048 B · no allocation failures"

    panel.reset()

    assert panel.main_live_label.text == "--"
    assert panel.ahb_live_label.text == "--"
    assert panel.status_label.text == "Power on to view LPC1768 memory usage."
    assert panel.copy_details_button.disabled


def test_memory_panel_copies_loaded_allocation_details(monkeypatch) -> None:
    panel = memory_panel()
    copied: list[str] = []
    monkeypatch.setattr("gui.views.memory_panel.ui.clipboard.write", copied.append)
    panel.details_label.text = "Main SRAM\n64 B · Robot"

    panel.copy_details()

    assert copied == ["Main SRAM\n64 B · Robot"]


def test_memory_panel_renders_allocation_groups_only_after_details_arrive() -> None:
    panel = memory_panel()
    details = MemoryDetails(
        summary=memory_summary(),
        allocation_groups=(
            MemoryAllocationGroup(
                region=MemoryRegion.MAIN_SRAM,
                type_name="Robot",
                host_payload_bytes=128,
                target_payload_bytes=64,
                live_count=1,
                peak_live_count=1,
                total_count=1,
                live_target_bytes=64,
                peak_target_bytes=64,
                target_size_exact=True,
            ),
            MemoryAllocationGroup(
                region=MemoryRegion.AHB_SRAM,
                type_name="raw buffer",
                host_payload_bytes=256,
                target_payload_bytes=256,
                live_count=2,
                peak_live_count=2,
                total_count=2,
                live_target_bytes=512,
                peak_target_bytes=512,
                target_size_exact=True,
            ),
            MemoryAllocationGroup(
                region=MemoryRegion.MAIN_SRAM,
                type_name="Planner",
                host_payload_bytes=184,
                target_payload_bytes=184,
                live_count=3,
                peak_live_count=4,
                total_count=5,
                live_target_bytes=552,
                peak_target_bytes=736,
                target_size_exact=True,
            ),
            MemoryAllocationGroup(
                region=MemoryRegion.AHB_SRAM,
                type_name="Block[]",
                host_payload_bytes=1_024,
                target_payload_bytes=1_024,
                live_count=1,
                peak_live_count=1,
                total_count=1,
                live_target_bytes=1_024,
                peak_target_bytes=1_024,
                target_size_exact=True,
            ),
        ),
    )

    panel.set_details(details)

    assert not panel.copy_details_button.disabled
    assert panel.details_label.text.splitlines() == [
        "Main SRAM",
        "552 B · Planner · 3 live × 184 B · peak 736 B (4) · 5 allocations",
        "64 B · Robot · 1 live × 64 B · peak 64 B (1) · 1 allocation",
        "",
        "AHB SRAM",
        "1,024 B · Block[] · 1 live × 1,024 B · peak 1,024 B (1) · 1 allocation",
        "512 B · raw buffer · 2 live × 256 B · peak 512 B (2) · 2 allocations",
    ]
