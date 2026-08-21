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

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

from nicegui import ui

from gui.protocol.model import MemoryDetails, MemoryRegion, MemorySummary


def format_bytes(value: int) -> str:
    return f"{value:,} B"


def _usage_text(used: int, capacity: int) -> str:
    return f"{used:,} / {capacity:,} B"


def _status_text(summary: MemorySummary) -> str:
    failures = summary.heap.failed_allocation_count
    unresolved = (
        summary.unresolved_main_live_host_bytes
        + summary.unresolved_ahb_live_host_bytes
        + summary.unresolved_heap_live_host_bytes
    )
    warnings: list[str] = []
    warnings.append(f"largest free block {format_bytes(summary.heap.largest_free_block_bytes)}")
    if failures:
        warnings.append(f"{failures} allocation failures")
    else:
        warnings.append("no allocation failures")
    if unresolved:
        warnings.append(f"{format_bytes(unresolved)} unresolved host allocations")
    return " · ".join(warnings)


def _details_text(details: MemoryDetails) -> str:
    if not details.allocation_groups:
        return "No live or historical allocation groups."

    sections: list[str] = []
    for region, title in (
        (MemoryRegion.MAIN_SRAM, "Main SRAM"),
        (MemoryRegion.AHB_SRAM, "AHB SRAM"),
        (MemoryRegion.UNIFIED_HEAP, "Unified heap (unplaced estimates)"),
    ):
        groups = sorted(
            (group for group in details.allocation_groups if group.region is region),
            key=lambda group: (-group.live_target_bytes, group.type_name),
        )
        lines = [title]
        if not groups:
            lines.append("No allocation groups.")
        for group in groups:
            estimate = "" if group.target_size_exact else " (host-size estimate)"
            allocation_word = "allocation" if group.total_count == 1 else "allocations"
            lines.append(
                f"{format_bytes(group.live_target_bytes)}{estimate} · {group.type_name} · "
                f"{group.live_count} live × {format_bytes(group.target_payload_bytes)} · "
                f"peak {format_bytes(group.peak_target_bytes)} ({group.peak_live_count}) · "
                f"{group.total_count} {allocation_word}"
            )
        sections.append("\n".join(lines))
    return "\n\n".join(sections)


@dataclass
class MemoryPanelView:
    heap_used_label: Any
    heap_peak_label: Any
    heap_free_label: Any
    heap_min_free_label: Any
    main_used_label: Any
    main_free_label: Any
    ahb_used_label: Any
    ahb_free_label: Any
    status_label: Any
    details_label: Any
    copy_details_button: Any

    def update_summary(self, summary: MemorySummary) -> None:
        heap_used = summary.heap.capacity_bytes - summary.heap.total_free_bytes
        self.heap_used_label.text = _usage_text(heap_used, summary.heap.capacity_bytes)
        self.heap_peak_label.text = format_bytes(summary.heap.peak_live_payload_bytes)
        self.heap_free_label.text = format_bytes(summary.heap.total_free_bytes)
        self.heap_min_free_label.text = format_bytes(summary.heap.minimum_ever_free_bytes)
        main_capacity = summary.main.heap_committed_bytes + summary.main.total_free_bytes
        self.main_used_label.text = _usage_text(summary.main.heap_committed_bytes, main_capacity)
        self.main_free_label.text = format_bytes(summary.main.total_free_bytes)
        ahb_used = summary.ahb.live_payload_bytes + summary.ahb.allocator_overhead_bytes
        self.ahb_used_label.text = _usage_text(ahb_used, summary.ahb.dynamic_capacity_bytes)
        self.ahb_free_label.text = format_bytes(summary.ahb.total_free_bytes)
        self.status_label.text = _status_text(summary)

    def set_details(self, details: MemoryDetails) -> None:
        self.update_summary(details.summary)
        self.details_label.text = _details_text(details)
        self.copy_details_button.enable()

    def copy_details(self) -> None:
        ui.clipboard.write(str(self.details_label.text))

    def reset(self) -> None:
        for label in (
            self.heap_used_label,
            self.heap_peak_label,
            self.heap_free_label,
            self.heap_min_free_label,
            self.main_used_label,
            self.main_free_label,
            self.ahb_used_label,
            self.ahb_free_label,
        ):
            label.text = "--"
        self.status_label.text = "Power on to view LPC1768 memory usage."
        self.details_label.text = "Request allocation details to inspect tracked types."
        self.copy_details_button.disable()


def _metric(name: str) -> Any:
    with ui.element("div").classes("metric"):
        ui.label(name).classes("metric-name")
        return ui.label("--").classes("metric-value")


def build_memory_panel(*, refresh_details: Callable[[], Awaitable[None]]) -> MemoryPanelView:
    with ui.element("div").classes("panel-section"):
        ui.label("Memory").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            heap_used_label = _metric("Unified heap used")
            heap_peak_label = _metric("Unified payload peak")
            heap_free_label = _metric("Unified heap free")
            heap_min_free_label = _metric("Minimum ever free")
            main_used_label = _metric("Main SRAM heap used")
            main_free_label = _metric("Main SRAM heap free")
            ahb_used_label = _metric("AHB SRAM heap used")
            ahb_free_label = _metric("AHB SRAM heap free")
        status_label = ui.label("Power on to view LPC1768 memory usage.").classes("section-subtle")
        with ui.row().classes("items-center"):
            ui.button("Allocation details", icon="memory", on_click=refresh_details).props("dense outline")
            copy_details_button = ui.button("Copy details", icon="content_copy").props("dense outline")
            copy_details_button.disable()
        details_label = ui.label("Request allocation details to inspect tracked types.").classes(
            "section-subtle memory-details"
        )

    panel = MemoryPanelView(
        heap_used_label=heap_used_label,
        heap_peak_label=heap_peak_label,
        heap_free_label=heap_free_label,
        heap_min_free_label=heap_min_free_label,
        main_used_label=main_used_label,
        main_free_label=main_free_label,
        ahb_used_label=ahb_used_label,
        ahb_free_label=ahb_free_label,
        status_label=status_label,
        details_label=details_label,
        copy_details_button=copy_details_button,
    )
    copy_details_button.on_click(panel.copy_details)
    return panel
