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


ROOT = Path(__file__).resolve().parents[2]


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def _source_files(*roots: str) -> list[Path]:
    return sorted(path for root in roots for path in (ROOT / root).rglob("*") if path.suffix in {".cpp", ".hpp", ".h"})


def _assert_pattern_absent(paths: list[Path], pattern: str, message: str) -> None:
    offenders = [path.relative_to(ROOT) for path in paths if pattern in path.read_text(encoding="utf-8")]
    if offenders:
        formatted = "\n".join(f"  - {path}" for path in offenders)
        raise SystemExit(f"{message}\n{formatted}")


def test_runtime_sources_do_not_rewrite_firmware_access_specifiers() -> None:
    _assert_pattern_absent(
        _source_files("src", "include"),
        "#define private public",
        "runtime code should not rewrite firmware private/protected access with macros",
    )


def test_physical_boundary_uses_physical_io_instead_of_firmware_interpreted_state() -> None:
    if "PublicData::get_value(pwm_spindle_control_checksum" in _read("src/runtime/runtime_modules.cpp"):
        raise SystemExit("spindle tach simulation should derive RPM from PWM output, not spindle PublicData")
    if "PublicData::get_value(zprobe_checksum" in _read("src/runtime/runtime_physical_controls.cpp"):
        raise SystemExit("probe input API should read physical GPIO, not ZProbe PublicData")
    if "kernel.config" in _read("src/core/event_engine.cpp"):
        raise SystemExit("event engine should not parse firmware config while servicing the physical model")
    physical_scene = _read("src/core/physical_scene.cpp")
    if "Kernel&" in physical_scene or "kernel." in physical_scene:
        raise SystemExit("PhysicalScene should consume plain simulator config, not Kernel internals")
    if "gpio::" in physical_scene or "board_profile" in physical_scene:
        raise SystemExit("PhysicalScene should delegate GPIO driving to PhysicalSignalDriver")
    if "stock_z_probe_contacts" in physical_scene or "sphere_contacts_box" in physical_scene:
        raise SystemExit("PhysicalScene should delegate probe geometry to ProbeContactModel")


def test_firmware_runtime_facade_delegates_boot_session_work() -> None:
    facade = _read("src/runtime/firmware_runtime.cpp")
    forbidden = {
        "std::make_unique<Kernel>": "Kernel construction belongs in the runtime boot session",
        "runtime_modules::load_firmware_modules": "module loading belongs in the runtime boot session",
        "Kernel::instance = nullptr": "firmware singleton reset belongs in the runtime boot session",
        "m8266_wifi::": "Wi-Fi byte queues belong in RuntimeIo",
        "system_reset::": "reset detection belongs in RuntimePump",
        "kernel.conveyor": "motion pump details belong in RuntimePump",
        "drive_configured_input": "physical input wiring belongs in RuntimePhysicalControls",
    }
    for pattern, message in forbidden.items():
        if pattern in facade:
            raise SystemExit(message)


def test_core_simulator_state_lives_in_simulator_context() -> None:
    forbidden = [
        ("src/core/virtual_clock.cpp", "VirtualClock"),
        ("src/core/lpc1768_sim.cpp", "Lpc1768"),
        ("src/core/stepper_axis.cpp", "StepperAxisRegistry"),
        ("src/core/timer_scheduler.cpp", "TimerScheduler"),
        ("src/core/interrupt_controller.cpp", "InterruptController"),
        ("src/core/realtime_timer_pacer.cpp", "RealtimeTimerPacer"),
        ("src/core/motion_telemetry.cpp", "MotionTelemetry"),
        ("src/core/physical_scene.cpp", "PhysicalScene"),
        ("src/core/main_button_led.cpp", "MainButtonLedState"),
        ("src/core/spindle_state.cpp", "SpindleState"),
        ("src/core/us_ticker_sim.cpp", "UsTickerState"),
        ("src/core/adc_sim.cpp", "AdcState"),
        ("src/core/mbed_peripheral_state.cpp", "PwmOutRegistry"),
        ("src/core/mbed_peripheral_state.cpp", "InterruptInRegistry"),
        ("src/protocol/m8266_wifi.cpp", "Module"),
        ("src/runtime/runtime_motor_alarm_wiring.cpp", "AlarmSignal"),
    ]
    for relative_path, type_name in forbidden:
        if f"static {type_name}" in _read(relative_path):
            raise SystemExit(f"{type_name} should be owned by SimulatorContext, not a file-local singleton")
    if "Module module;" in _read("src/protocol/m8266_wifi.cpp"):
        raise SystemExit("M8266 Wi-Fi module should be owned by SimulatorContext, not a file-local singleton")
    if "alarm_signals{}" in _read("src/runtime/runtime_motor_alarm_wiring.cpp"):
        raise SystemExit("motor alarm wiring should be owned by SimulatorContext, not a file-local singleton")
    if "I2cEepromDevice device;" in _read("src/firmware/i2c_sim.cpp"):
        raise SystemExit("I2C EEPROM state should be owned by SimulatorContext, not a file-local singleton")
    if "static std::map<std::string, std::filesystem::path> mount_table" in _read("src/firmware/host_filesystem.cpp"):
        raise SystemExit("host filesystem mounts should be owned by SimulatorContext, not a file-local singleton")


def test_interactive_transports_share_nonblocking_fd_pump() -> None:
    transport_files = [
        "src/protocol/localhost_tcp_bridge.cpp",
        "src/protocol/virtual_com_port.cpp",
    ]
    for relative_path in transport_files:
        text = _read(relative_path)
        for pattern in (
            "platform_io::read_available",
            "platform_io::drain_write_buffer",
            "std::this_thread::sleep_for",
        ):
            if pattern in text:
                raise SystemExit(f"{relative_path} should delegate fd read/write polling to NonblockingFdPump")


def test_gui_runtime_owns_shared_state_through_explicit_session() -> None:
    runtime = _read("gui/app_runtime.py")
    page = _read("gui/app_page.py")
    session = _read("gui/core/session.py")
    if "session = SimulatorSession.create(" not in runtime:
        raise SystemExit("GUI runtime should own shared simulator state through one explicit session object")
    if "class AppView" in runtime:
        raise SystemExit("AppView should live in gui/app_view.py, not the runtime entrypoint")
    if "from gui.app_view import AppView" not in page:
        raise SystemExit("page construction should import AppView from its own module")
    if "from gui.app_actions import AppActions" not in runtime:
        raise SystemExit("GUI runtime should delegate command callbacks to AppActions")
    if "from gui.app_page import build_ui_page" not in runtime:
        raise SystemExit("GUI runtime should delegate page construction to app_page")
    for owned_in_session in ("SimulatorClient(", "TelemetryBuffer(", "TransportLogStore(", "GuiStateStore("):
        if owned_in_session in runtime:
            raise SystemExit(f"GUI runtime should get {owned_in_session} through SimulatorSession")
    for pattern in (
        "SimulatorClient(",
        "TelemetryBuffer(",
        "TransportLogStore(",
        "GuiStateStore(",
        "SimulatorProcessController(",
    ):
        if pattern not in session:
            raise SystemExit(f"SimulatorSession should own {pattern}")


def test_python_tests_are_pytest_native() -> None:
    main_function = "def " + "main("
    main_guard = "__" + "main__"
    offenders: list[str] = []
    for path in sorted((ROOT / "gui/tests").glob("*_test.py")):
        text = path.read_text(encoding="utf-8")
        if main_function in text or main_guard in text:
            offenders.append(str(path.relative_to(ROOT)))
    if offenders:
        formatted = "\n".join(f"  - {path}" for path in offenders)
        raise SystemExit(f"Python tests should be pytest-collected, not standalone script runners:\n{formatted}")


def test_cpp_tests_use_unique_temp_directories() -> None:
    offenders: list[str] = []
    for path in _source_files("tests"):
        if path.name == "temp_sdcard.hpp":
            continue
        text = path.read_text(encoding="utf-8")
        if "std::filesystem::temp_directory_path() /" in text:
            offenders.append(str(path.relative_to(ROOT)))
    if offenders:
        formatted = "\n".join(f"  - {path}" for path in offenders)
        raise SystemExit(f"C++ tests should use TempDirectory/TempSdCard instead of fixed temp paths:\n{formatted}")


def test_cmake_auto_registers_simple_cpp_tests() -> None:
    cmake = _read("cmake/TestsAndApps.cmake")
    required = (
        "file(GLOB SIM_ALL_TEST_SOURCES CONFIGURE_DEPENDS",
        "SIM_RUNTIME_TESTS",
        "SIM_SPECIAL_TESTS",
        "SIM_FIRMWARE_SIM_TESTS",
    )
    for pattern in required:
        if pattern not in cmake:
            raise SystemExit(f"CMake should auto-register simple tests through {pattern}")
