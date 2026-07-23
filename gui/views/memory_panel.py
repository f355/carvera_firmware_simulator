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
    status = (
        f"Config cache {format_bytes(summary.main.config_cache_bytes)}"
        if summary.main.config_cache_active
        else "Config cache inactive"
    )
    failures = summary.main.failed_allocation_count + summary.ahb.failed_allocation_count
    unresolved = summary.unresolved_main_live_host_bytes + summary.unresolved_ahb_live_host_bytes
    warnings: list[str] = []
    if summary.main.config_cache_collision:
        warnings.append("config cache collision")
    if summary.main.heap_limit_collision:
        warnings.append("heap limit collision")
    if failures:
        warnings.append(f"{failures} allocation failures")
    else:
        warnings.append("no allocation failures")
    if unresolved:
        warnings.append(f"{format_bytes(unresolved)} unresolved host allocations")
    return " · ".join((status, *warnings))


def _details_text(details: MemoryDetails) -> str:
    if not details.allocation_groups:
        return "No live or historical allocation groups."
    lines: list[str] = []
    for group in details.allocation_groups:
        region = {
            MemoryRegion.MAIN_SRAM: "Main SRAM",
            MemoryRegion.AHB_SRAM: "AHB SRAM",
        }.get(group.region, "Unknown SRAM")
        estimate = "" if group.target_size_exact else " estimated"
        lines.append(
            f"{region} · {group.type_name}\n"
            f"  live {group.live_count} × {format_bytes(group.target_payload_bytes)}"
            f" = {format_bytes(group.live_target_bytes)}{estimate}; "
            f"peak {group.peak_live_count} / {format_bytes(group.peak_target_bytes)}; "
            f"total allocations {group.total_count}"
        )
    return "\n".join(lines)


@dataclass
class MemoryPanelView:
    main_live_label: Any
    main_peak_label: Any
    main_free_label: Any
    main_margin_label: Any
    ahb_live_label: Any
    ahb_peak_label: Any
    ahb_free_label: Any
    ahb_largest_label: Any
    status_label: Any
    details_label: Any

    def update_summary(self, summary: MemorySummary) -> None:
        self.main_live_label.text = _usage_text(summary.main.live_payload_bytes, summary.main.capacity_bytes)
        self.main_peak_label.text = format_bytes(summary.main.peak_live_payload_bytes)
        self.main_free_label.text = format_bytes(summary.main.total_free_bytes)
        self.main_margin_label.text = format_bytes(summary.main.minimum_margin_bytes)
        self.ahb_live_label.text = _usage_text(summary.ahb.live_payload_bytes, summary.ahb.capacity_bytes)
        self.ahb_peak_label.text = format_bytes(summary.ahb.peak_live_payload_bytes)
        self.ahb_free_label.text = format_bytes(summary.ahb.total_free_bytes)
        self.ahb_largest_label.text = format_bytes(summary.ahb.largest_free_block_bytes)
        self.status_label.text = _status_text(summary)

    def set_details(self, details: MemoryDetails) -> None:
        self.update_summary(details.summary)
        self.details_label.text = _details_text(details)

    def reset(self) -> None:
        for label in (
            self.main_live_label,
            self.main_peak_label,
            self.main_free_label,
            self.main_margin_label,
            self.ahb_live_label,
            self.ahb_peak_label,
            self.ahb_free_label,
            self.ahb_largest_label,
        ):
            label.text = "--"
        self.status_label.text = "Power on to view LPC1768 memory usage."
        self.details_label.text = "Request allocation details to inspect tracked types."


def _metric(name: str) -> Any:
    with ui.element("div").classes("metric"):
        ui.label(name).classes("metric-name")
        return ui.label("--").classes("metric-value")


def build_memory_panel(*, refresh_details: Callable[[], Awaitable[None]]) -> MemoryPanelView:
    with ui.element("div").classes("panel-section"):
        ui.label("Memory").classes("section-title")
        with ui.element("div").classes("metrics-grid"):
            main_live_label = _metric("Main SRAM live")
            main_peak_label = _metric("Main SRAM peak")
            main_free_label = _metric("Main SRAM free")
            main_margin_label = _metric("Minimum stack margin")
            ahb_live_label = _metric("AHB SRAM live")
            ahb_peak_label = _metric("AHB SRAM peak")
            ahb_free_label = _metric("AHB SRAM free")
            ahb_largest_label = _metric("Largest AHB block")
        status_label = ui.label("Power on to view LPC1768 memory usage.").classes("section-subtle")
        ui.button("Allocation details", icon="memory", on_click=refresh_details).props("dense outline")
        details_label = ui.label("Request allocation details to inspect tracked types.").classes(
            "section-subtle memory-details"
        )

    return MemoryPanelView(
        main_live_label=main_live_label,
        main_peak_label=main_peak_label,
        main_free_label=main_free_label,
        main_margin_label=main_margin_label,
        ahb_live_label=ahb_live_label,
        ahb_peak_label=ahb_peak_label,
        ahb_free_label=ahb_free_label,
        ahb_largest_label=ahb_largest_label,
        status_label=status_label,
        details_label=details_label,
    )
