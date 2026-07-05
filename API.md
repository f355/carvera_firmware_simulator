# Simulator API

This is the external API for tests, scripts, and GUI-like tools that want to
drive the simulator without linking C++ code.

The schema lives in [proto/carvera_sim.proto](proto/carvera_sim.proto), package
`carvera.sim.v1`. Treat the `.proto` file as the source of truth for field
numbers and exact message shapes; this document explains how the pieces are
supposed to be used.

## Shape of the API

The API is hardware-shaped, not controller-shaped. It lets clients manipulate
the simulated physical world around the firmware:

* mount an SD card directory
* choose C1 or CA1 before boot
* start a virtual COM port and localhost Wi-Fi endpoint
* press the main button, e-stop, cover switch, and other physical inputs
* put tools in the spindle or C1 ATC rack
* define stock geometry
* set thermistor temperatures and alarm inputs
* read physical axes, spindle RPM, laser state, GPIO/PWM state, structured EEPROM contents,
  tool state, and controller traffic

For normal use, prefer the physical-machine commands. The low-level harness
commands exist for focused peripheral tests and should not be the first thing a
GUI or black-box test reaches for.

## Transports

There are two stdio transports. Both use the same input framing:

```text
uint32_le protobuf_size
protobuf Request bytes
```

Frames are capped at 16 MiB.

`carvera_sim_stdio` is request/response only:

```text
stdin:  Request
stdout: Response
```

`carvera_sim_stream_stdio` is the preferred transport for interactive tools:

```text
stdin:  Request
stdout: StreamFrame
```

A `StreamFrame` contains either a `Response` or an asynchronous `Event`. Clients
waiting for a response must ignore unrelated events and match responses by
`Response.id`.

The GUI uses the streaming transport because it needs movement telemetry while
the firmware is busy homing, probing, running ATC, or servicing controller
traffic.

## Requests and responses

Every `Request` has a client-chosen `id`. The matching `Response` copies that
ID, sets `ok`, and either carries a typed payload or an error string.

Only one command is set per request, through the `Request.command` `oneof`.
Only one payload is set per response, through the `Response.payload` `oneof`.

The simulator process owns one runtime. `Reset` resets that runtime and any
booted firmware instance; it does not make a new OS process.

## Basic startup

A typical external client does this:

1. Start `build/carvera_sim_stream_stdio`.
2. Send `SetMachineModel` before anything boots the firmware.
3. Send `MountFilesystem` for `sd`.
4. Optionally set EEPROM contents, rack tools, rotary accessory state, or other
   physical setup.
5. Send `StartInteractiveTransport` if a controller should connect.
6. Send `SetTimeMode(TIME_MODE_REALTIME)` for interactive use, or keep manual
   time for deterministic tests.
7. Read stream events: fast `MachineTelemetry` for motion, slower
   `MachineSnapshot` for firmware/status metadata, and `PhysicalIoSnapshot` for
   inputs/outputs.

`GetMachineSnapshot`, serial commands, jog commands, and interactive transport
startup may lazily boot the firmware. Anything that must affect factory/boot
state should be sent before that.

## Command groups

### Lifecycle and time

* `Reset`: reset simulator and firmware runtime.
* `GetStatus`: read virtual time, time mode, realtime speed, machine model, and `FuncSetting`.
* `SetMachineModel`: choose C1/CA1 and factory function byte before boot.
* `SetTimeMode`: choose manual deterministic time or realtime wall-clock time.
* `SetRealtimeSpeed`: scale realtime virtual time against wall-clock time.
* `AdvanceTime`: advance manual-mode virtual time.
* `Poll`: dispatch pending simulator work in realtime mode.

Manual time is meant for tests. Realtime mode is meant for the GUI and the
controller, where the simulated machine should keep moving while nobody is
explicitly advancing time. Realtime speed is useful for fast interactive
dry-runs; controller I/O still runs on normal host wall-clock time.

### Host filesystem

`MountFilesystem` maps a firmware mount name such as `sd` to a host directory.
Firmware paths under `/sd/...` are then redirected there.

This is a file API boundary, not an SD-card block-device API. There is no FAT,
SPI, DMA, or USB mass-storage emulation below it.

### Controller-facing links

`StartInteractiveTransport` exposes endpoints on the same runtime used by the
protobuf API:

* `enable_uart`: create a POSIX pty-backed virtual COM port where supported
* `tcp_ports`: create all-interface TCP endpoints for the fake Wi-Fi link; `0`
  asks the OS for an ephemeral port
* `log_traffic`: mirror sanitized UART/Wi-Fi RX/TX bytes to stderr

The response payload is `InteractiveTransport`, containing the pty path and
bound TCP endpoints. A `0.0.0.0` host means the bridge accepts connections
through any IPv4 address assigned to the simulator host. On macOS, Linux, and
WSL, the UART path is a PTY path such as `/dev/ttys123` or `/dev/pts/7`. The TCP
endpoints are the preferred controller path on every platform. Native Windows builds and
native `COMx` devices are intentionally unsupported; run the simulator inside
WSL2 on Windows.

`StopInteractiveTransport` closes those endpoints without resetting firmware.

While interactive transport is running, controller bytes still pass through the
real firmware serial/Wi-Fi path. The host IO threads only drain and fill byte
queues so sockets stay alive while firmware code is busy.

### Firmware serial helpers

These commands are useful for tests that want a black-box-ish firmware path
without creating OS sockets:

* `WriteSerial`: inject bytes into the primary firmware serial RX path.
* `ReadSerial`: drain firmware serial TX bytes.
* `RunUntilIdle`: run serial handling and motion until idle or budget exhausted.
* `Jog`: convenience helper that formats a relative G-code move and still enters
  firmware through the serial path.

For controller integration, use `StartInteractiveTransport` instead.

### Physical inputs and setup

These commands describe the simulated physical machine:

* `SetCoverOpen`
* `SetMainButtonPressed`
* `SetEStopPressed`
* `SetMotorAlarm`
* `SetSpindleAlarm`
* `SetRotaryAccessoryInstalled`
* `SetTemperature`
* `SetStockBox`
* `SetAtcPocketTools`
* `SetSpindleTool`
* `SetProbeToolInstalled`
* `SetEepromContents`
* `SetEepromBytes`

Tool length is the overall tool length. The simulator assumes 20 mm of shank is
inside the spindle collet and uses the remaining stickout for physical contact
and visualization.

`SetSpindleTool` is the manual-tool path: use it to put a virtual tool in the
spindle before confirming a CA1/manual tool change. `SetAtcPocketTools` is the
C1 rack path.

`SetRotaryAccessoryInstalled` is deliberately separate from EEPROM. Plugging in
the physical A-axis accessory does not rewrite factory settings on a real
machine either.

### Readbacks

Useful readbacks:

* `GetMachineSnapshot`: firmware boot state, homed state, firmware soft limits,
  physical travel, axes, spindle state, ATC/tool state, and ETS geometry
* `GetFrontPanelState`: main button, e-stop, 12V/24V rails, C1 direct RGB GPIO
  LED state, and CA1 LED-strip colors captured at the firmware LED helper
* `GetProbeInputs`: probe and electronic tool setter input states as seen by
  firmware
* `GetCoverOpen`
* `GetLimitSwitch`
* `GetMotorAlarm`
* `GetSpindleAlarm`
* `GetSwitchState`
* `GetPwmOutput`
* `GetLaserState`
* `GetAdcInput`
* `GetEepromContents`
* `GetEepromBytes`

For drawing the machine, use `AxisState.physical_mm`, not
`AxisState.machine_position`. Physical position is counted below Robot from
step/dir pulses. `machine_position` is firmware G53/logical state and can differ
for good reasons.

`MachineSnapshot.work_area` is firmware soft-limit geometry.
`MachineSnapshot.physical_travel` is simulator-owned hardware geometry.
`MachineSnapshot.axes` exists for one-shot API clients and initial state, but
live visual clients should treat `MachineTelemetry.axes` as authoritative once
telemetry is flowing. The streamed full snapshot is slower and can be stale by
the time a motion frame has already arrived.

### Streaming events

`carvera_sim_stream_stdio` can emit asynchronous events:

* `MachineTelemetry`: fast physical motion, spindle RPM, and ATC physical state
  changes
* `MachineSnapshot`: slower firmware/status snapshot with soft limits, physical
  travel, axes, spindle state, ATC/tool state, and ETS geometry
* `PhysicalIoSnapshot`: slower physical input/output snapshot for front-panel,
  probe/ETS, cover, alarms, PWM, switch, and laser indicators
* `Status`: reserved for simulator status updates

Movement telemetry is not a heartbeat. If nothing physical changed, no movement
frame is emitted.

Telemetry includes physical axes and physical travel so a visual client can
initialize its transform before rendering motion. It also includes spindle RPM
and ATC state so tool visuals can follow clamp handoff timing rather than
waiting for a slower full snapshot.

For GUI-style clients, a useful rule is: let `MachineTelemetry` own physical
motion, and let `MachineSnapshot`/`PhysicalIoSnapshot` own slower metadata and
indicators.

## Low-level harness commands

These commands intentionally punch below the physical-machine surface:

* `SetGpioInput`
* `GetGpioLevel`
* `AttachStepDirAxis`
* `GetAxisPosition`
* `TriggerInterruptRise`
* `SetProbeInputs`
* `SetToolSetterBox`
* `SetAdcInput`
* `SetSwitchState`

They are useful for peripheral tests and narrow regression cases. They are not
how a real machine is operated, and they are usually the wrong level for GUI
features.

## Minimal Python framing example

The generated Python module used by the GUI lives in `gui/generated`. A tiny
client looks like this:

```python
import struct
import subprocess

from gui.generated import carvera_sim_pb2 as pb


def write_msg(stream, msg):
    payload = msg.SerializeToString()
    stream.write(struct.pack("<I", len(payload)))
    stream.write(payload)
    stream.flush()


def read_msg(stream, cls):
    size = struct.unpack("<I", stream.read(4))[0]
    msg = cls()
    msg.ParseFromString(stream.read(size))
    return msg


proc = subprocess.Popen(
    ["build/carvera_sim_stdio"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
)

request = pb.Request(id=1, get_status=pb.GetStatus())
write_msg(proc.stdin, request)
response = read_msg(proc.stdout, pb.Response)

assert response.id == 1
assert response.ok, response.error
print(response.status)
```

For `carvera_sim_stream_stdio`, read `pb.StreamFrame` instead of `pb.Response`
and handle both `frame.response` and `frame.event`.

## Internal C++ boundary

The external API is implemented by `sim::ApiService` in `src/protocol/`. It is
transport-neutral; stdio, stream-stdio, and future transports should sit around
that service rather than reaching into firmware modules directly.

The rough source split is:

```text
src/firmware/   firmware-facing host shims and portability hooks
src/core/       simulator-owned peripherals, time, machine physics, board data
src/runtime/    firmware boot/runtime glue
src/protocol/   protobuf service, framing, controller transports
src/apps/       executable entry points
```

Firmware-facing LPC/CMSIS/mbed symbols remain adapter glue. New simulator
behavior should generally live in C++ simulator classes and be exposed through
domain-shaped protobuf commands only when an external client needs it.
