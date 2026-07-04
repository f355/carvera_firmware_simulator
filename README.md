# Carvera Firmware Simulator

**WARNING: This is a simulator, not a safety device, not an LPC1768 emulator, and
not proof that a real machine will behave exactly the same way. It is meant for
firmware development, controller integration, and automated tests. Do not use it
as the last line of defense before doing something expensive or stupid with a
real CNC machine.**

This repository builds the Carvera firmware as a host executable and surrounds it
with simulated LPC1768-ish peripherals, a physical machine model, a virtual SD
card/EEPROM, controller-facing serial links, and a Python GUI.

The goal is to run as much of the real stock C1/CA1 firmware as practical while
keeping the boundary below the firmware modules people normally work on.

Useful companion docs:

* [API.md](API.md): protobuf API used by tests and the GUI
* [LICENSE.md](LICENSE.md): GNU GPLv3

## Current status

The simulator can boot the firmware, home, jog, talk to the Makera controller
over localhost Wi-Fi or a POSIX virtual COM port, load per-machine SD card
configs, persist EEPROM, run Player from host-backed files, simulate the spindle
PWM/tach path, laser state, probing, ETS contact, C1 ATC tool changes, CA1
manual tool changes, front-panel inputs, e-stop, cover, motor alarms, thermistor
inputs, fan outputs, and the optional rotary A-axis at a basic homing/jogging
level. Free-running mode can also be sped up for interactive dry-runs.

The GUI is a machine simulator surface, not a controller replacement.
Controller commands should still go through the virtual COM port or fake Wi-Fi
endpoint.

## Installing dependencies

You need:

* CMake
* a C++20 compiler
* Protobuf headers, libraries, and `protoc`
* Python 3.11+
* [uv](https://docs.astral.sh/uv/) for the GUI environment
* Git

### macOS

This is the most exercised setup.

```sh
xcode-select --install
brew install cmake ninja protobuf uv
```

### Linux

Linux should work, but is not regularly tested.

Debian/Ubuntu-ish:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build protobuf-compiler libprotobuf-dev python3 git curl
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Fedora-ish:

```sh
sudo dnf install gcc-c++ cmake ninja-build protobuf-compiler protobuf-devel python3 git
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### Windows

Windows support means WSL2. Native Windows builds are intentionally unsupported;
we do not maintain a separate MSVC or MinGW build path.

```powershell
wsl --install
```

Then install the Linux dependencies inside WSL and build/run the simulator from
the WSL shell.

## Building

The simulator is an independent CMake project. By default it uses the firmware
revision pinned in [firmware/compatible_commit](firmware/compatible_commit). To
prepare that checkout explicitly:

```sh
./scripts/ensure_firmware_checkout.sh
```

That clones [the configured repository](firmware/repository_url) into
`firmware/Carvera_Community_Firmware/` and checks out the compatible commit.
After that, a normal build can use the managed checkout automatically:

```sh
cmake -S . -B build -G Ninja
cmake --build build
cmake --build build --target check
```

To build against your own firmware checkout instead, pass it explicitly:

```sh
cmake -S . -B build -G Ninja -DCARVERA_FIRMWARE_ROOT=/path/to/Carvera_Community_Firmware
cmake --build build
cmake --build build --target check
```

User-provided firmware checkouts are checked against
`firmware/compatible_commit`. If you are deliberately developing against a
different firmware revision, add `-DCARVERA_ALLOW_UNPINNED_FIRMWARE=ON`.

If you do not use Ninja, drop `-G Ninja`.

### Coverage reports

Configure a separate instrumented build and run its `coverage` target:

```sh
cmake -S . -B build-coverage -G Ninja -DCARVERA_SIM_ENABLE_COVERAGE=ON
cmake --build build-coverage --target coverage
```

The target runs the C++ test suite and writes annotated firmware and simulator
reports to `build-coverage/coverage/`. The firmware report includes only the
firmware sources compiled for the C1 and CA1 simulator configurations. Coverage
is informational; the target does not enforce a threshold.

## Running the GUI

The easy path is:

```sh
./run_gui.sh
```

If no firmware checkout is specified, `run_gui.sh` clones or updates the managed
checkout at the pinned compatible commit. Use `--firmware-root` when you want to
run against an existing checkout, or `--firmware-dir` when you want the managed
clone somewhere else.

`--firmware-root` also enforces the compatible commit. Set
`CARVERA_ALLOW_UNPINNED_FIRMWARE=1` when intentionally running the simulator
against a different firmware revision.

Useful options:

```sh
./run_gui.sh --firmware-root /path/to/Carvera_Community_Firmware
./run_gui.sh --firmware-dir /path/to/managed/clone
./run_gui.sh --model ca1
./run_gui.sh --port 8090
./run_gui.sh --wifi-port 2223
./run_gui.sh --no-open
./run_gui.sh --no-log-transport
```

The script configures CMake, builds the GUI runtime binary, syncs the `uv`
environment, starts NiceGUI, and opens `http://127.0.0.1:8080` unless
`--no-open` is passed.

The GUI launches the simulator when the power switch is turned on. The firmware
then boots, loads the selected machine config, and homes itself through the real
firmware startup path.

## Controller connections

While powered on, the GUI shows the fake Wi-Fi address and POSIX virtual COM
port path. The fake Wi-Fi endpoint is also advertised on the
controller discovery port, so the Makera controller should be able to find it by
scan or connect to `127.0.0.1:<port>` manually.

The fake Wi-Fi/TCP endpoint is the recommended controller connection on all
platforms. The serial endpoint is a POSIX pseudo-terminal on macOS, Linux, and
WSL; on Linux/WSL it may not appear in the controller's serial dropdown because
the controller lists `serial.tools.list_ports.comports()` devices rather than
every PTY under `/dev/pts`. Native Windows `COMx` devices are not provided by
the simulator.

UART/Wi-Fi traffic is mirrored to the terminal by default. Use
`--no-log-transport` if that gets too chatty.

The lower-level runner is useful when you do not want the GUI:

```sh
./build/carvera_sim_interactive --sd /path/to/sd --model ca1 --wifi-port 0
```

It prints endpoints like:

```text
UART /dev/ttys123
WIFI 127.0.0.1:54321
```

The UART endpoint is a POSIX pseudo-terminal on macOS/Linux. The Wi-Fi endpoint
is a localhost TCP bridge through the real firmware `WifiProvider` path.

## Persistent machine state

The simulator keeps virtual machine state per model:

```text
sdcard/c1/
sdcard/ca1/
```

Those directories are ignored by Git. If a model SD directory is empty when the
GUI starts, it is initialized from:

```text
default_sdcard/c1/
default_sdcard/ca1/
```

Controller-uploaded programs live under `gcodes/` inside the selected SD card,
matching the path used by the firmware `Player`.

EEPROM lives next to the matching virtual SD card as `.eeprom.bin`, so all
persistent state stays together. Runtime resets preserve that state, just like
power-cycling a real machine does not magically erase its EEPROM.

## What this does not emulate

This is the important part.

* It does not emulate the LPC1768 CPU, instruction timing, memory bus, or exact
  ABI. The firmware is recompiled as host code.
* LPC1768 peripherals are behavioral C++ shims, not silicon. GPIO, PINCON,
  timers, NVIC, ADC, PWM, I2C, watchdog, and `us_ticker` are modeled only as
  deeply as current firmware workflows need.
* Interrupts are not real Cortex-M preemption. Firmware ISRs are real, but the
  simulator dispatches pending interrupts through its scheduler in priority
  order. Host OS timing and scheduling can still leak into free-running mode.
* Time has two modes: deterministic/manual advancement for tests and
  free-running OS-paced time for interactive use. Neither is cycle-perfect MCU
  time.
* Controller PTY/TCP endpoints use host IO threads. Those threads only move
  bytes into queues; firmware-visible serial/Wi-Fi state is still owned by the
  runtime thread.
* `/sd` is host-backed file IO. There is no SD electrical model, SPI block
  transport, FAT implementation, DMA, or USB mass-storage sector emulation.
* EEPROM is a file-backed byte array behind the firmware I2C calls. Timing,
  wear, brown-outs, and weird EEPROM failure modes are not modeled.
* Wi-Fi is modeled at the logical ESP8266/M8266 protocol level below the real
  firmware `WifiProvider`. ESP firmware, RF behavior, and SPI timing are not
  emulated.
* The C1 wireless probe link is modeled as a logical second-UART/Zigbee peer,
  not as a radio.
* ADC inputs are simulator-provided raw values plus stock firmware conversion
  code. Analog noise and board-level electrical behavior are out of scope.
* PWM and SoftPWM expose duty, period, and logical output state. They are not
  waveform-accurate.
* Physical motion comes from emitted step/dir pulses, not from directly trusting
  `Robot` machine coordinates. That is intentional, but the physical world is
  still simplified.
* The GUI/protobuf API can set physical inputs, tools, stock, temperatures, and
  alarms directly for tests. That is a simulator control surface, not a real
  firmware feature.
* Reset, MRI/debug hooks, AHB SRAM placement, and watchdog resets are host
  integration shims.

In other words: this is a firmware-and-machine simulator, not a microcontroller
emulator. It tries to be honest where the firmware meets the machine, not where
an LPC1768 meets a logic analyzer.

## Limited or missing firmware features

Known gaps worth remembering:

* USB host and USB-drive G-code playback are stubbed. Stock C1/CA1 machines do
  not appear to use this path.
* Native Windows builds are not supported. Use WSL2 on Windows.
* Front-panel LED colors are captured at the firmware helper/function-call
  level. The CA1 LED-strip bit-banged waveform timing is intentionally not
  decoded.
* Rotary A-axis support covers plugged/unplugged state, homing, jogging, and
  visualization. It does not yet model a rotary workpiece, collisions, or
  contact behavior.
* Probing and ATC cover the main happy paths, including C1 rack tool changes,
  CA1 manual tool changes, ETS touch-off, stock probing, and 3D probe side
  contact. More failure/recovery cases still need tests.
* Things unused by Makera machines are not covered: SCARA, rotary-delta,
  non-Cartesian kinematics, non-CartGrid leveling, Modbus spindle, PID autotune,
  and B-axis machine behavior.
* Canned drilling cycles are linked when configured by the firmware, but stock
  C1/CA1 configs do not enable them and the simulator does not spend much effort
  there.

## Source layout

```text
src/firmware/   firmware-facing host shims and portability hooks
src/core/       simulator-owned peripherals, time, machine physics, board data
src/runtime/    firmware boot/runtime glue
src/protocol/   protobuf, controller transports, logical Wi-Fi/Zigbee devices
src/apps/       small executable entry points
gui/            Python/NiceGUI frontend
proto/          public protobuf schema
tests/          C++ tests
```

Firmware source discovery is in [cmake/FirmwareSources.cmake](cmake/FirmwareSources.cmake).
It auto-detects source files in the supported C1/CA1 firmware subsystems and
keeps explicit exclusions for unsupported or host-copied firmware pieces.

## External API

The protobuf schema is [proto/carvera_sim.proto](proto/carvera_sim.proto).

`carvera_sim_stdio` reads little-endian length-prefixed protobuf `Request`
messages on stdin and writes length-prefixed `Response` messages on stdout. The
same dispatcher backs the GUI.

The API is intentionally hardware-shaped: configure physical tools, stock,
machine inputs, temperatures, alarms, and realtime speed; read physical axes,
GPIO/PWM, laser/spindle state, probe contacts, telemetry, EEPROM fields, and
controller traffic.

## Useful development commands

For a fresh checkout, sync the Python environment and regenerate the protobuf
bindings before running GUI checks:

```sh
uv sync
uv run python -m gui.protocol.proto_codegen
```

```sh
cmake --build build -j8
ctest --test-dir build --output-on-failure -j8
ctest --test-dir build -L fast --output-on-failure -j8
ctest --test-dir build -L integration --output-on-failure -j8
cmake --build build --target check

uv run ruff format gui
uv run ruff check gui
uv run mypy gui
PYTHONPATH=. uv run pytest gui/tests
```

## License

The simulator is licensed under the GNU GPLv3; see [LICENSE.md](LICENSE.md).
That matches the linked Smoothieware/Carvera firmware sources, which are also
GPLv3-family code.
