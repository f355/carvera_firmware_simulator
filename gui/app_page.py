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

from typing import Any

from nicegui import ui

from gui.app_view import AppView
from gui.core.defaults import PWM_WATCHES, SWITCH_WATCHES
from gui.core.platform_paths import open_in_file_manager
from gui.core.session import SimulatorSession
from gui.presenters.app import AppPresenters
from gui.scene.lighting import configure_scene_lighting
from gui.scene.machine_scene_view import MachineSceneView
from gui.views.atc_tab import build_atc_tab
from gui.views.comms_log_tab import build_comms_log_tab
from gui.views.eeprom_panel import build_eeprom_panel
from gui.views.environment_tab import build_environment_tab
from gui.views.io_panel import IoPanelView
from gui.views.machine_status import AtcPanelView, AxisPanelView
from gui.views.machine_tab import build_firmware_state_panel, build_machine_tab
from gui.views.memory_panel import build_memory_panel
from gui.views.signals_tab import build_signals_tab
from gui.views.stock_tab import build_stock_tab
from gui.views.styles import SIM_CSS
from gui.views.transport_panel import build_transport_panel
from gui.views.ui_helpers import event_value

ORTHOGRAPHIC_CAMERA_SIZE_MM = 780.0


def build_ui_page(session: SimulatorSession, actions: AppPresenters) -> None:
    view = AppView()
    gui_state = session.state_store.snapshot()
    selected_model = gui_state.machine_model or session.args.model

    def machine_model_value() -> str:
        assert view.model_select is not None
        return str(view.model_select.value)

    def scene_appearance_event(event: Any = None) -> None:
        actions.appearance.scene_appearance_changed(view, event)

    def machine_model_changed() -> None:
        actions.state.update_machine_shell_model(view)
        actions.tooling.load_default_stock(view)

    ui.add_css(SIM_CSS)
    with ui.element("div").classes("sim-page"):
        with ui.element("div").classes("sim-toolbar"):
            ui.label("Carvera Simulator").classes("sim-title")
            view.header.model_select = ui.toggle(
                {"c1": "Carvera (C1)", "ca1": "Carvera Air (CA1)"},
                value=selected_model,
                on_change=lambda _: machine_model_changed(),
            ).props("dense unelevated toggle-color=primary")
            view.header.cad_models_switch = ui.switch(
                "Show 3D Machine", value=True, on_change=lambda event: actions.state.cad_models_changed(view, event)
            )
            with ui.element("div").classes("header-controls"):
                with ui.element("div").classes("axis-strip"):
                    axis_labels = {}
                    for axis in ("X", "Y", "Z", "A"):
                        with ui.element("div").classes("axis-readout") as axis_readout:
                            view.header.axis_readouts[axis] = axis_readout
                            ui.label(axis).classes("axis-name")
                            axis_labels[axis] = ui.label("--").classes("axis-value")
                with ui.element("div").classes("primary-controls"):
                    view.header.main_button_control = (
                        ui.button("Main", on_click=lambda: actions.physical.press_main_button(view))
                        .props("dense outline")
                        .classes("main-button-led")
                    )
                    e_stop_switch = ui.switch(
                        "E-stop",
                        value=False,
                        on_change=lambda event: actions.physical.front_panel_changed(view, "e_stop", event),
                    )
                    view.header.e_stop_switch = e_stop_switch
                    view.header.power_switch = ui.switch(
                        "Power", value=False, on_change=lambda event: actions.power.power_changed(view, event)
                    )
        with ui.splitter(value=64).classes("main-splitter") as main_splitter:
            with main_splitter.before:
                with ui.element("div").classes("scene-pane"):
                    machine_scene = (
                        ui.scene(
                            width=1200,
                            height=760,
                            grid=False,
                            camera=ui.scene.orthographic_camera(size=ORTHOGRAPHIC_CAMERA_SIZE_MM, far=5000),
                            background_color="#f6f7f9",
                            control_type="orbit",
                            fps=30,
                        )
                        .classes("machine-scene")
                        .style("width: 100%;")
                    )
                    with ui.element("div").classes("scene-action-bar"):
                        ui.button(
                            "Clear Backplot",
                            icon="timeline",
                            on_click=lambda: actions.state.clear_backplot(view),
                        ).props("dense outline")
                    machine_scene.move_camera(x=430, y=-560, z=320, look_at_x=0, look_at_y=0, look_at_z=70, duration=0)
                    machine_scene.axes_helper(35)
                    configure_scene_lighting(machine_scene)
            with main_splitter.after:
                with ui.element("div").classes("side-panel"):
                    with ui.tabs().classes("side-tabs w-full") as tabs:
                        status_tab = ui.tab("Status")
                        environment_tab = ui.tab("Environment")
                        tool_table_tab = ui.tab("Tool Table")
                        stock_tab = ui.tab("Stock")
                        service_tab = ui.tab("Service")
                        comms_tab = ui.tab("Comms Log")

                    with ui.tab_panels(tabs, value=status_tab).classes("side-panels w-full"):
                        with ui.tab_panel(status_tab):
                            view.machine_tab_view = build_machine_tab(
                                cover_changed=lambda event: actions.physical.cover_changed(view, event),
                                rotary_accessory_changed=lambda event: actions.physical.rotary_accessory_changed(
                                    view, event
                                ),
                            )
                            assert view.rotary_accessory_switch is not None
                            view.signals_tab_view = build_signals_tab(
                                switch_watches=SWITCH_WATCHES,
                                pwm_watches=PWM_WATCHES,
                            )

                        with ui.tab_panel(environment_tab):
                            view.environment_tab_view = build_environment_tab(
                                motor_alarm_changed=lambda axis, event: actions.physical.motor_alarm_changed(
                                    view, axis, event
                                ),
                                spindle_alarm_changed=lambda event: actions.physical.spindle_alarm_changed(view, event),
                                realtime_speed_changed=lambda event: actions.physical.realtime_speed_changed(
                                    view, event_value(event)
                                ),
                                scene_appearance_changed=scene_appearance_event,
                                set_temperature=lambda: actions.physical.set_temperature(view),
                            )

                        with ui.tab_panel(tool_table_tab):
                            view.atc_tab_view = build_atc_tab(
                                apply_rack=lambda: actions.tooling.apply_atc_table(view),
                                load_pocket=lambda pocket: actions.tooling.load_spindle_tool_from_pocket(view, pocket),
                                unload_spindle=lambda: actions.tooling.unload_spindle_tool(view),
                                load_defaults=lambda: actions.tooling.load_default_tools(view),
                                clear_loaded=lambda: actions.tooling.clear_tool_table(view),
                            )

                        with ui.tab_panel(stock_tab):
                            view.stock_tab_view = build_stock_tab(
                                apply_stock=lambda: actions.tooling.apply_physical_boxes(view),
                                machine_model=selected_model,
                            )

                        with ui.tab_panel(service_tab):
                            view.firmware_state_view = build_firmware_state_panel()
                            view.transport_panel_view = build_transport_panel(
                                sd_root=session.sd_root,
                                simulator_path=session.args.simulator,
                                open_sd_root=lambda: open_in_file_manager(session.sd_root),
                            )
                            view.memory_panel_view = build_memory_panel(
                                refresh_details=lambda: actions.service.refresh_memory_details(view),
                            )
                            view.eeprom_panel_view = build_eeprom_panel(
                                refresh_eeprom=lambda: actions.service.refresh_eeprom(view),
                                write_eeprom=lambda: actions.service.write_eeprom(view),
                            )

                        with ui.tab_panel(comms_tab):
                            view.comms_log_view = build_comms_log_tab()

        view.axis_panel_view = AxisPanelView(
            axis_labels=axis_labels,
            axis_detail_labels=view.axis_detail_labels,
            axis_endstop_badges=view.axis_endstop_badges,
        )
        assert view.atc_tab_view is not None
        view.atc_panel_view = AtcPanelView(
            available_badge=view.atc_tab_view.available_badge,
            active_tool_label=view.atc_tab_view.active_tool_label,
            target_tool_label=view.atc_tab_view.target_tool_label,
            tlo_label=view.atc_tab_view.tlo_label,
            held_length_label=view.atc_tab_view.held_length_label,
            tool_rows=view.tool_rows,
        )
        assert view.temperature_status is not None
        assert view.machine_tab_view is not None
        assert view.environment_tab_view is not None
        assert view.signals_tab_view is not None
        view.io_panel_view = IoPanelView(
            machine_model_value=machine_model_value,
            pwm_watches=PWM_WATCHES,
            probe_badge=view.machine_tab_view.probe_badge,
            tool_setter_badge=view.machine_tab_view.tool_setter_badge,
            cover_switch=view.machine_tab_view.cover_switch,
            cover_badge=view.machine_tab_view.cover_badge,
            temperature_status=view.temperature_status,
            front_panel_controls=view.front_panel_controls,
            front_panel_badges=view.front_panel_badges,
            motor_alarm_switches=view.motor_alarm_switches,
            motor_alarm_badges=view.motor_alarm_badges,
            spindle_alarm_switch=view.environment_tab_view.spindle_alarm_switch,
            spindle_alarm_badge=[
                view.machine_tab_view.spindle_alarm_badge,
                view.environment_tab_view.spindle_alarm_badge,
            ],
            firmware_switch_controls=view.firmware_switch_controls,
            laser_badges=view.laser_badges,
            laser_power_label=view.signals_tab_view.laser_power_label,
            pwm_labels=view.pwm_labels,
            main_button_control=view.main_button_control,
            ca1_led_strip_indicators=view.signals_tab_view.front_panel_badges.get("led_strip", ()),
        )
        assert view.transport_panel_view is not None
        view.machine_scene_view = MachineSceneView(
            scene=machine_scene,
            model_asset_for=session.machine_model_registry.asset_for,
            machine_model_label=view.transport_panel_view.machine_model_label,
            axis_mode_select=view.rotary_accessory_switch,
            front_panel_badges=view.front_panel_badges,
            axis_readouts=view.axis_readouts,
            axis_detail_rows=view.axis_detail_rows,
            machine_model_scale=float(session.args.machine_model_scale),
            model_offset_override=session.machine_model_offset_override,
            model_rotation_override=session.machine_model_rotation_override,
            backplot_history=session.backplot_history,
        )
        assert view.cad_models_switch is not None
        view.machine_scene_view.set_cad_models_visible(bool(view.cad_models_switch.value))
        actions.state.update_machine_shell_model(view)
        actions.state.restore_view(view)

    ui.timer(0.033, lambda: actions.state.drain_telemetry(view))
    ui.timer(0.1, lambda: actions.state.drain_snapshots(view))
    ui.timer(0.1, lambda: actions.state.drain_physical_io(view))
    ui.timer(0.1, lambda: actions.state.drain_transport_log(view))
