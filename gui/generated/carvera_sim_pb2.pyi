# mypy: disable-error-code="var-annotated"
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class TimeMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TIME_MODE_UNSPECIFIED: _ClassVar[TimeMode]
    TIME_MODE_MANUAL: _ClassVar[TimeMode]
    TIME_MODE_REALTIME: _ClassVar[TimeMode]

class Axis(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    AXIS_UNSPECIFIED: _ClassVar[Axis]
    AXIS_X: _ClassVar[Axis]
    AXIS_Y: _ClassVar[Axis]
    AXIS_Z: _ClassVar[Axis]
    AXIS_A: _ClassVar[Axis]
    AXIS_B: _ClassVar[Axis]

class LimitSwitchSide(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    LIMIT_SWITCH_SIDE_UNSPECIFIED: _ClassVar[LimitSwitchSide]
    LIMIT_SWITCH_SIDE_MIN: _ClassVar[LimitSwitchSide]
    LIMIT_SWITCH_SIDE_MAX: _ClassVar[LimitSwitchSide]

class TemperatureSensor(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TEMPERATURE_SENSOR_UNSPECIFIED: _ClassVar[TemperatureSensor]
    TEMPERATURE_SENSOR_SPINDLE: _ClassVar[TemperatureSensor]
    TEMPERATURE_SENSOR_POWER: _ClassVar[TemperatureSensor]

class SwitchName(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    SWITCH_NAME_UNSPECIFIED: _ClassVar[SwitchName]
    SWITCH_NAME_LIGHT: _ClassVar[SwitchName]
    SWITCH_NAME_VACUUM: _ClassVar[SwitchName]
    SWITCH_NAME_SPINDLE_FAN: _ClassVar[SwitchName]
    SWITCH_NAME_POWER_FAN: _ClassVar[SwitchName]
    SWITCH_NAME_TOOL_SENSOR: _ClassVar[SwitchName]
    SWITCH_NAME_PROBE_CHARGER: _ClassVar[SwitchName]
    SWITCH_NAME_AIR: _ClassVar[SwitchName]
    SWITCH_NAME_BEEP: _ClassVar[SwitchName]

class MachineModel(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    MACHINE_MODEL_UNSPECIFIED: _ClassVar[MachineModel]
    MACHINE_MODEL_CARVERA_C1: _ClassVar[MachineModel]
    MACHINE_MODEL_CARVERA_AIR_CA1: _ClassVar[MachineModel]

class ToolKind(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TOOL_KIND_UNSPECIFIED: _ClassVar[ToolKind]
    TOOL_KIND_CUTTING_TOOL: _ClassVar[ToolKind]
    TOOL_KIND_STOCK_Z_PROBE: _ClassVar[ToolKind]
    TOOL_KIND_THREE_AXIS_PROBE: _ClassVar[ToolKind]

class EepromFieldType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    EEPROM_FIELD_TYPE_UNSPECIFIED: _ClassVar[EepromFieldType]
    EEPROM_FIELD_TYPE_FLOAT: _ClassVar[EepromFieldType]
    EEPROM_FIELD_TYPE_INT: _ClassVar[EepromFieldType]
    EEPROM_FIELD_TYPE_BOOL: _ClassVar[EepromFieldType]
TIME_MODE_UNSPECIFIED: TimeMode
TIME_MODE_MANUAL: TimeMode
TIME_MODE_REALTIME: TimeMode
AXIS_UNSPECIFIED: Axis
AXIS_X: Axis
AXIS_Y: Axis
AXIS_Z: Axis
AXIS_A: Axis
AXIS_B: Axis
LIMIT_SWITCH_SIDE_UNSPECIFIED: LimitSwitchSide
LIMIT_SWITCH_SIDE_MIN: LimitSwitchSide
LIMIT_SWITCH_SIDE_MAX: LimitSwitchSide
TEMPERATURE_SENSOR_UNSPECIFIED: TemperatureSensor
TEMPERATURE_SENSOR_SPINDLE: TemperatureSensor
TEMPERATURE_SENSOR_POWER: TemperatureSensor
SWITCH_NAME_UNSPECIFIED: SwitchName
SWITCH_NAME_LIGHT: SwitchName
SWITCH_NAME_VACUUM: SwitchName
SWITCH_NAME_SPINDLE_FAN: SwitchName
SWITCH_NAME_POWER_FAN: SwitchName
SWITCH_NAME_TOOL_SENSOR: SwitchName
SWITCH_NAME_PROBE_CHARGER: SwitchName
SWITCH_NAME_AIR: SwitchName
SWITCH_NAME_BEEP: SwitchName
MACHINE_MODEL_UNSPECIFIED: MachineModel
MACHINE_MODEL_CARVERA_C1: MachineModel
MACHINE_MODEL_CARVERA_AIR_CA1: MachineModel
TOOL_KIND_UNSPECIFIED: ToolKind
TOOL_KIND_CUTTING_TOOL: ToolKind
TOOL_KIND_STOCK_Z_PROBE: ToolKind
TOOL_KIND_THREE_AXIS_PROBE: ToolKind
EEPROM_FIELD_TYPE_UNSPECIFIED: EepromFieldType
EEPROM_FIELD_TYPE_FLOAT: EepromFieldType
EEPROM_FIELD_TYPE_INT: EepromFieldType
EEPROM_FIELD_TYPE_BOOL: EepromFieldType

class Request(_message.Message):
    __slots__ = ("id", "reset", "poll", "get_status", "set_time_mode", "advance_time", "set_realtime_speed", "set_machine_model", "mount_filesystem", "start_interactive_transport", "stop_interactive_transport", "write_serial", "read_serial", "run_until_idle", "jog", "set_cover_open", "get_cover_open", "set_motor_alarm", "get_motor_alarm", "set_spindle_alarm", "get_spindle_alarm", "set_atc_pocket_tools", "set_main_button_pressed", "set_e_stop_pressed", "get_front_panel_state", "set_temperature", "set_limit_switch", "get_limit_switch", "set_spindle_tool", "set_probe_tool_installed", "set_stock_box", "get_machine_snapshot", "get_pwm_output", "get_switch_state", "get_laser_state", "set_rotary_accessory_installed", "get_eeprom_bytes", "set_eeprom_bytes", "get_eeprom_fields", "set_eeprom_fields", "get_probe_inputs", "get_adc_input", "set_gpio_input", "get_gpio_level", "attach_step_dir_axis", "get_axis_position", "trigger_interrupt_rise", "set_probe_inputs", "set_tool_setter_box", "set_adc_input", "set_switch_state")
    ID_FIELD_NUMBER: _ClassVar[int]
    RESET_FIELD_NUMBER: _ClassVar[int]
    POLL_FIELD_NUMBER: _ClassVar[int]
    GET_STATUS_FIELD_NUMBER: _ClassVar[int]
    SET_TIME_MODE_FIELD_NUMBER: _ClassVar[int]
    ADVANCE_TIME_FIELD_NUMBER: _ClassVar[int]
    SET_REALTIME_SPEED_FIELD_NUMBER: _ClassVar[int]
    SET_MACHINE_MODEL_FIELD_NUMBER: _ClassVar[int]
    MOUNT_FILESYSTEM_FIELD_NUMBER: _ClassVar[int]
    START_INTERACTIVE_TRANSPORT_FIELD_NUMBER: _ClassVar[int]
    STOP_INTERACTIVE_TRANSPORT_FIELD_NUMBER: _ClassVar[int]
    WRITE_SERIAL_FIELD_NUMBER: _ClassVar[int]
    READ_SERIAL_FIELD_NUMBER: _ClassVar[int]
    RUN_UNTIL_IDLE_FIELD_NUMBER: _ClassVar[int]
    JOG_FIELD_NUMBER: _ClassVar[int]
    SET_COVER_OPEN_FIELD_NUMBER: _ClassVar[int]
    GET_COVER_OPEN_FIELD_NUMBER: _ClassVar[int]
    SET_MOTOR_ALARM_FIELD_NUMBER: _ClassVar[int]
    GET_MOTOR_ALARM_FIELD_NUMBER: _ClassVar[int]
    SET_SPINDLE_ALARM_FIELD_NUMBER: _ClassVar[int]
    GET_SPINDLE_ALARM_FIELD_NUMBER: _ClassVar[int]
    SET_ATC_POCKET_TOOLS_FIELD_NUMBER: _ClassVar[int]
    SET_MAIN_BUTTON_PRESSED_FIELD_NUMBER: _ClassVar[int]
    SET_E_STOP_PRESSED_FIELD_NUMBER: _ClassVar[int]
    GET_FRONT_PANEL_STATE_FIELD_NUMBER: _ClassVar[int]
    SET_TEMPERATURE_FIELD_NUMBER: _ClassVar[int]
    SET_LIMIT_SWITCH_FIELD_NUMBER: _ClassVar[int]
    GET_LIMIT_SWITCH_FIELD_NUMBER: _ClassVar[int]
    SET_SPINDLE_TOOL_FIELD_NUMBER: _ClassVar[int]
    SET_PROBE_TOOL_INSTALLED_FIELD_NUMBER: _ClassVar[int]
    SET_STOCK_BOX_FIELD_NUMBER: _ClassVar[int]
    GET_MACHINE_SNAPSHOT_FIELD_NUMBER: _ClassVar[int]
    GET_PWM_OUTPUT_FIELD_NUMBER: _ClassVar[int]
    GET_SWITCH_STATE_FIELD_NUMBER: _ClassVar[int]
    GET_LASER_STATE_FIELD_NUMBER: _ClassVar[int]
    SET_ROTARY_ACCESSORY_INSTALLED_FIELD_NUMBER: _ClassVar[int]
    GET_EEPROM_BYTES_FIELD_NUMBER: _ClassVar[int]
    SET_EEPROM_BYTES_FIELD_NUMBER: _ClassVar[int]
    GET_EEPROM_FIELDS_FIELD_NUMBER: _ClassVar[int]
    SET_EEPROM_FIELDS_FIELD_NUMBER: _ClassVar[int]
    GET_PROBE_INPUTS_FIELD_NUMBER: _ClassVar[int]
    GET_ADC_INPUT_FIELD_NUMBER: _ClassVar[int]
    SET_GPIO_INPUT_FIELD_NUMBER: _ClassVar[int]
    GET_GPIO_LEVEL_FIELD_NUMBER: _ClassVar[int]
    ATTACH_STEP_DIR_AXIS_FIELD_NUMBER: _ClassVar[int]
    GET_AXIS_POSITION_FIELD_NUMBER: _ClassVar[int]
    TRIGGER_INTERRUPT_RISE_FIELD_NUMBER: _ClassVar[int]
    SET_PROBE_INPUTS_FIELD_NUMBER: _ClassVar[int]
    SET_TOOL_SETTER_BOX_FIELD_NUMBER: _ClassVar[int]
    SET_ADC_INPUT_FIELD_NUMBER: _ClassVar[int]
    SET_SWITCH_STATE_FIELD_NUMBER: _ClassVar[int]
    id: int
    reset: Reset
    poll: Poll
    get_status: GetStatus
    set_time_mode: SetTimeMode
    advance_time: AdvanceTime
    set_realtime_speed: SetRealtimeSpeed
    set_machine_model: SetMachineModel
    mount_filesystem: MountFilesystem
    start_interactive_transport: StartInteractiveTransport
    stop_interactive_transport: StopInteractiveTransport
    write_serial: WriteSerial
    read_serial: ReadSerial
    run_until_idle: RunUntilIdle
    jog: Jog
    set_cover_open: SetCoverOpen
    get_cover_open: GetCoverOpen
    set_motor_alarm: SetMotorAlarm
    get_motor_alarm: GetMotorAlarm
    set_spindle_alarm: SetSpindleAlarm
    get_spindle_alarm: GetSpindleAlarm
    set_atc_pocket_tools: SetAtcPocketTools
    set_main_button_pressed: SetMainButtonPressed
    set_e_stop_pressed: SetEStopPressed
    get_front_panel_state: GetFrontPanelState
    set_temperature: SetTemperature
    set_limit_switch: SetLimitSwitch
    get_limit_switch: GetLimitSwitch
    set_spindle_tool: SetSpindleTool
    set_probe_tool_installed: SetProbeToolInstalled
    set_stock_box: SetStockBox
    get_machine_snapshot: GetMachineSnapshot
    get_pwm_output: GetPwmOutput
    get_switch_state: GetSwitchState
    get_laser_state: GetLaserState
    set_rotary_accessory_installed: SetRotaryAccessoryInstalled
    get_eeprom_bytes: GetEepromBytes
    set_eeprom_bytes: SetEepromBytes
    get_eeprom_fields: GetEepromFields
    set_eeprom_fields: SetEepromFields
    get_probe_inputs: GetProbeInputs
    get_adc_input: GetAdcInput
    set_gpio_input: SetGpioInput
    get_gpio_level: GetGpioLevel
    attach_step_dir_axis: AttachStepDirAxis
    get_axis_position: GetAxisPosition
    trigger_interrupt_rise: TriggerInterruptRise
    set_probe_inputs: SetProbeInputs
    set_tool_setter_box: SetToolSetterBox
    set_adc_input: SetAdcInput
    set_switch_state: SetSwitchState
    def __init__(self, id: _Optional[int] = ..., reset: _Optional[_Union[Reset, _Mapping]] = ..., poll: _Optional[_Union[Poll, _Mapping]] = ..., get_status: _Optional[_Union[GetStatus, _Mapping]] = ..., set_time_mode: _Optional[_Union[SetTimeMode, _Mapping]] = ..., advance_time: _Optional[_Union[AdvanceTime, _Mapping]] = ..., set_realtime_speed: _Optional[_Union[SetRealtimeSpeed, _Mapping]] = ..., set_machine_model: _Optional[_Union[SetMachineModel, _Mapping]] = ..., mount_filesystem: _Optional[_Union[MountFilesystem, _Mapping]] = ..., start_interactive_transport: _Optional[_Union[StartInteractiveTransport, _Mapping]] = ..., stop_interactive_transport: _Optional[_Union[StopInteractiveTransport, _Mapping]] = ..., write_serial: _Optional[_Union[WriteSerial, _Mapping]] = ..., read_serial: _Optional[_Union[ReadSerial, _Mapping]] = ..., run_until_idle: _Optional[_Union[RunUntilIdle, _Mapping]] = ..., jog: _Optional[_Union[Jog, _Mapping]] = ..., set_cover_open: _Optional[_Union[SetCoverOpen, _Mapping]] = ..., get_cover_open: _Optional[_Union[GetCoverOpen, _Mapping]] = ..., set_motor_alarm: _Optional[_Union[SetMotorAlarm, _Mapping]] = ..., get_motor_alarm: _Optional[_Union[GetMotorAlarm, _Mapping]] = ..., set_spindle_alarm: _Optional[_Union[SetSpindleAlarm, _Mapping]] = ..., get_spindle_alarm: _Optional[_Union[GetSpindleAlarm, _Mapping]] = ..., set_atc_pocket_tools: _Optional[_Union[SetAtcPocketTools, _Mapping]] = ..., set_main_button_pressed: _Optional[_Union[SetMainButtonPressed, _Mapping]] = ..., set_e_stop_pressed: _Optional[_Union[SetEStopPressed, _Mapping]] = ..., get_front_panel_state: _Optional[_Union[GetFrontPanelState, _Mapping]] = ..., set_temperature: _Optional[_Union[SetTemperature, _Mapping]] = ..., set_limit_switch: _Optional[_Union[SetLimitSwitch, _Mapping]] = ..., get_limit_switch: _Optional[_Union[GetLimitSwitch, _Mapping]] = ..., set_spindle_tool: _Optional[_Union[SetSpindleTool, _Mapping]] = ..., set_probe_tool_installed: _Optional[_Union[SetProbeToolInstalled, _Mapping]] = ..., set_stock_box: _Optional[_Union[SetStockBox, _Mapping]] = ..., get_machine_snapshot: _Optional[_Union[GetMachineSnapshot, _Mapping]] = ..., get_pwm_output: _Optional[_Union[GetPwmOutput, _Mapping]] = ..., get_switch_state: _Optional[_Union[GetSwitchState, _Mapping]] = ..., get_laser_state: _Optional[_Union[GetLaserState, _Mapping]] = ..., set_rotary_accessory_installed: _Optional[_Union[SetRotaryAccessoryInstalled, _Mapping]] = ..., get_eeprom_bytes: _Optional[_Union[GetEepromBytes, _Mapping]] = ..., set_eeprom_bytes: _Optional[_Union[SetEepromBytes, _Mapping]] = ..., get_eeprom_fields: _Optional[_Union[GetEepromFields, _Mapping]] = ..., set_eeprom_fields: _Optional[_Union[SetEepromFields, _Mapping]] = ..., get_probe_inputs: _Optional[_Union[GetProbeInputs, _Mapping]] = ..., get_adc_input: _Optional[_Union[GetAdcInput, _Mapping]] = ..., set_gpio_input: _Optional[_Union[SetGpioInput, _Mapping]] = ..., get_gpio_level: _Optional[_Union[GetGpioLevel, _Mapping]] = ..., attach_step_dir_axis: _Optional[_Union[AttachStepDirAxis, _Mapping]] = ..., get_axis_position: _Optional[_Union[GetAxisPosition, _Mapping]] = ..., trigger_interrupt_rise: _Optional[_Union[TriggerInterruptRise, _Mapping]] = ..., set_probe_inputs: _Optional[_Union[SetProbeInputs, _Mapping]] = ..., set_tool_setter_box: _Optional[_Union[SetToolSetterBox, _Mapping]] = ..., set_adc_input: _Optional[_Union[SetAdcInput, _Mapping]] = ..., set_switch_state: _Optional[_Union[SetSwitchState, _Mapping]] = ...) -> None: ...

class Response(_message.Message):
    __slots__ = ("id", "ok", "error", "status", "gpio_level", "attached_axis", "axis_position", "serial_data", "run_result", "jog_result", "pwm_output", "adc_input", "probe_inputs", "machine_snapshot", "interactive_transport", "cover_state", "motor_alarm_state", "front_panel_state", "switch_state", "limit_switch_state", "spindle_alarm_state", "laser_state", "eeprom_bytes", "eeprom_fields")
    ID_FIELD_NUMBER: _ClassVar[int]
    OK_FIELD_NUMBER: _ClassVar[int]
    ERROR_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    GPIO_LEVEL_FIELD_NUMBER: _ClassVar[int]
    ATTACHED_AXIS_FIELD_NUMBER: _ClassVar[int]
    AXIS_POSITION_FIELD_NUMBER: _ClassVar[int]
    SERIAL_DATA_FIELD_NUMBER: _ClassVar[int]
    RUN_RESULT_FIELD_NUMBER: _ClassVar[int]
    JOG_RESULT_FIELD_NUMBER: _ClassVar[int]
    PWM_OUTPUT_FIELD_NUMBER: _ClassVar[int]
    ADC_INPUT_FIELD_NUMBER: _ClassVar[int]
    PROBE_INPUTS_FIELD_NUMBER: _ClassVar[int]
    MACHINE_SNAPSHOT_FIELD_NUMBER: _ClassVar[int]
    INTERACTIVE_TRANSPORT_FIELD_NUMBER: _ClassVar[int]
    COVER_STATE_FIELD_NUMBER: _ClassVar[int]
    MOTOR_ALARM_STATE_FIELD_NUMBER: _ClassVar[int]
    FRONT_PANEL_STATE_FIELD_NUMBER: _ClassVar[int]
    SWITCH_STATE_FIELD_NUMBER: _ClassVar[int]
    LIMIT_SWITCH_STATE_FIELD_NUMBER: _ClassVar[int]
    SPINDLE_ALARM_STATE_FIELD_NUMBER: _ClassVar[int]
    LASER_STATE_FIELD_NUMBER: _ClassVar[int]
    EEPROM_BYTES_FIELD_NUMBER: _ClassVar[int]
    EEPROM_FIELDS_FIELD_NUMBER: _ClassVar[int]
    id: int
    ok: bool
    error: str
    status: Status
    gpio_level: GpioLevel
    attached_axis: AttachedAxis
    axis_position: AxisPosition
    serial_data: SerialData
    run_result: RunResult
    jog_result: JogResult
    pwm_output: PwmOutput
    adc_input: AdcInput
    probe_inputs: ProbeInputs
    machine_snapshot: MachineSnapshot
    interactive_transport: InteractiveTransport
    cover_state: CoverState
    motor_alarm_state: MotorAlarmState
    front_panel_state: FrontPanelState
    switch_state: SwitchState
    limit_switch_state: LimitSwitchState
    spindle_alarm_state: SpindleAlarmState
    laser_state: LaserState
    eeprom_bytes: EepromBytes
    eeprom_fields: EepromFields
    def __init__(self, id: _Optional[int] = ..., ok: _Optional[bool] = ..., error: _Optional[str] = ..., status: _Optional[_Union[Status, _Mapping]] = ..., gpio_level: _Optional[_Union[GpioLevel, _Mapping]] = ..., attached_axis: _Optional[_Union[AttachedAxis, _Mapping]] = ..., axis_position: _Optional[_Union[AxisPosition, _Mapping]] = ..., serial_data: _Optional[_Union[SerialData, _Mapping]] = ..., run_result: _Optional[_Union[RunResult, _Mapping]] = ..., jog_result: _Optional[_Union[JogResult, _Mapping]] = ..., pwm_output: _Optional[_Union[PwmOutput, _Mapping]] = ..., adc_input: _Optional[_Union[AdcInput, _Mapping]] = ..., probe_inputs: _Optional[_Union[ProbeInputs, _Mapping]] = ..., machine_snapshot: _Optional[_Union[MachineSnapshot, _Mapping]] = ..., interactive_transport: _Optional[_Union[InteractiveTransport, _Mapping]] = ..., cover_state: _Optional[_Union[CoverState, _Mapping]] = ..., motor_alarm_state: _Optional[_Union[MotorAlarmState, _Mapping]] = ..., front_panel_state: _Optional[_Union[FrontPanelState, _Mapping]] = ..., switch_state: _Optional[_Union[SwitchState, _Mapping]] = ..., limit_switch_state: _Optional[_Union[LimitSwitchState, _Mapping]] = ..., spindle_alarm_state: _Optional[_Union[SpindleAlarmState, _Mapping]] = ..., laser_state: _Optional[_Union[LaserState, _Mapping]] = ..., eeprom_bytes: _Optional[_Union[EepromBytes, _Mapping]] = ..., eeprom_fields: _Optional[_Union[EepromFields, _Mapping]] = ...) -> None: ...

class StreamFrame(_message.Message):
    __slots__ = ("response", "event")
    RESPONSE_FIELD_NUMBER: _ClassVar[int]
    EVENT_FIELD_NUMBER: _ClassVar[int]
    response: Response
    event: Event
    def __init__(self, response: _Optional[_Union[Response, _Mapping]] = ..., event: _Optional[_Union[Event, _Mapping]] = ...) -> None: ...

class Event(_message.Message):
    __slots__ = ("sequence", "status", "machine_telemetry", "machine_snapshot", "physical_io")
    SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    MACHINE_TELEMETRY_FIELD_NUMBER: _ClassVar[int]
    MACHINE_SNAPSHOT_FIELD_NUMBER: _ClassVar[int]
    PHYSICAL_IO_FIELD_NUMBER: _ClassVar[int]
    sequence: int
    status: Status
    machine_telemetry: MachineTelemetry
    machine_snapshot: MachineSnapshot
    physical_io: PhysicalIoSnapshot
    def __init__(self, sequence: _Optional[int] = ..., status: _Optional[_Union[Status, _Mapping]] = ..., machine_telemetry: _Optional[_Union[MachineTelemetry, _Mapping]] = ..., machine_snapshot: _Optional[_Union[MachineSnapshot, _Mapping]] = ..., physical_io: _Optional[_Union[PhysicalIoSnapshot, _Mapping]] = ...) -> None: ...

class Reset(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class Poll(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class GetStatus(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class SetTimeMode(_message.Message):
    __slots__ = ("mode",)
    MODE_FIELD_NUMBER: _ClassVar[int]
    mode: TimeMode
    def __init__(self, mode: _Optional[_Union[TimeMode, str]] = ...) -> None: ...

class SetRealtimeSpeed(_message.Message):
    __slots__ = ("multiplier",)
    MULTIPLIER_FIELD_NUMBER: _ClassVar[int]
    multiplier: float
    def __init__(self, multiplier: _Optional[float] = ...) -> None: ...

class AdvanceTime(_message.Message):
    __slots__ = ("delta_us",)
    DELTA_US_FIELD_NUMBER: _ClassVar[int]
    delta_us: int
    def __init__(self, delta_us: _Optional[int] = ...) -> None: ...

class MountFilesystem(_message.Message):
    __slots__ = ("name", "host_path")
    NAME_FIELD_NUMBER: _ClassVar[int]
    HOST_PATH_FIELD_NUMBER: _ClassVar[int]
    name: str
    host_path: str
    def __init__(self, name: _Optional[str] = ..., host_path: _Optional[str] = ...) -> None: ...

class SetGpioInput(_message.Message):
    __slots__ = ("pin", "high")
    PIN_FIELD_NUMBER: _ClassVar[int]
    HIGH_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    high: bool
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ..., high: _Optional[bool] = ...) -> None: ...

class GetGpioLevel(_message.Message):
    __slots__ = ("pin",)
    PIN_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ...) -> None: ...

class AttachStepDirAxis(_message.Message):
    __slots__ = ("step_pin", "direction_pin", "invert_direction")
    STEP_PIN_FIELD_NUMBER: _ClassVar[int]
    DIRECTION_PIN_FIELD_NUMBER: _ClassVar[int]
    INVERT_DIRECTION_FIELD_NUMBER: _ClassVar[int]
    step_pin: PinAddress
    direction_pin: PinAddress
    invert_direction: bool
    def __init__(self, step_pin: _Optional[_Union[PinAddress, _Mapping]] = ..., direction_pin: _Optional[_Union[PinAddress, _Mapping]] = ..., invert_direction: _Optional[bool] = ...) -> None: ...

class GetAxisPosition(_message.Message):
    __slots__ = ("axis",)
    AXIS_FIELD_NUMBER: _ClassVar[int]
    axis: int
    def __init__(self, axis: _Optional[int] = ...) -> None: ...

class WriteSerial(_message.Message):
    __slots__ = ("data",)
    DATA_FIELD_NUMBER: _ClassVar[int]
    data: str
    def __init__(self, data: _Optional[str] = ...) -> None: ...

class ReadSerial(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class RunUntilIdle(_message.Message):
    __slots__ = ("max_step_ticks",)
    MAX_STEP_TICKS_FIELD_NUMBER: _ClassVar[int]
    max_step_ticks: int
    def __init__(self, max_step_ticks: _Optional[int] = ...) -> None: ...

class Jog(_message.Message):
    __slots__ = ("delta", "feed_rate", "max_step_ticks")
    DELTA_FIELD_NUMBER: _ClassVar[int]
    FEED_RATE_FIELD_NUMBER: _ClassVar[int]
    MAX_STEP_TICKS_FIELD_NUMBER: _ClassVar[int]
    delta: _containers.RepeatedCompositeFieldContainer[AxisDelta]
    feed_rate: float
    max_step_ticks: int
    def __init__(self, delta: _Optional[_Iterable[_Union[AxisDelta, _Mapping]]] = ..., feed_rate: _Optional[float] = ..., max_step_ticks: _Optional[int] = ...) -> None: ...

class SetMachineModel(_message.Message):
    __slots__ = ("machine_model", "function_setting")
    MACHINE_MODEL_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_SETTING_FIELD_NUMBER: _ClassVar[int]
    machine_model: MachineModel
    function_setting: int
    def __init__(self, machine_model: _Optional[_Union[MachineModel, str]] = ..., function_setting: _Optional[int] = ...) -> None: ...

class GetPwmOutput(_message.Message):
    __slots__ = ("pin",)
    PIN_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ...) -> None: ...

class TriggerInterruptRise(_message.Message):
    __slots__ = ("pin",)
    PIN_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ...) -> None: ...

class SetProbeInputs(_message.Message):
    __slots__ = ("probe", "tool_setter")
    PROBE_FIELD_NUMBER: _ClassVar[int]
    TOOL_SETTER_FIELD_NUMBER: _ClassVar[int]
    probe: bool
    tool_setter: bool
    def __init__(self, probe: _Optional[bool] = ..., tool_setter: _Optional[bool] = ...) -> None: ...

class GetProbeInputs(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class SetCoverOpen(_message.Message):
    __slots__ = ("open",)
    OPEN_FIELD_NUMBER: _ClassVar[int]
    open: bool
    def __init__(self, open: _Optional[bool] = ...) -> None: ...

class GetCoverOpen(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class CoverState(_message.Message):
    __slots__ = ("open",)
    OPEN_FIELD_NUMBER: _ClassVar[int]
    open: bool
    def __init__(self, open: _Optional[bool] = ...) -> None: ...

class SetLimitSwitch(_message.Message):
    __slots__ = ("axis", "side", "triggered")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    SIDE_FIELD_NUMBER: _ClassVar[int]
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    side: LimitSwitchSide
    triggered: bool
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., side: _Optional[_Union[LimitSwitchSide, str]] = ..., triggered: _Optional[bool] = ...) -> None: ...

class GetLimitSwitch(_message.Message):
    __slots__ = ("axis", "side")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    SIDE_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    side: LimitSwitchSide
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., side: _Optional[_Union[LimitSwitchSide, str]] = ...) -> None: ...

class LimitSwitchState(_message.Message):
    __slots__ = ("axis", "side", "triggered")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    SIDE_FIELD_NUMBER: _ClassVar[int]
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    side: LimitSwitchSide
    triggered: bool
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., side: _Optional[_Union[LimitSwitchSide, str]] = ..., triggered: _Optional[bool] = ...) -> None: ...

class SetMotorAlarm(_message.Message):
    __slots__ = ("axis", "triggered")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    triggered: bool
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., triggered: _Optional[bool] = ...) -> None: ...

class GetMotorAlarm(_message.Message):
    __slots__ = ("axis",)
    AXIS_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ...) -> None: ...

class MotorAlarmState(_message.Message):
    __slots__ = ("axis", "triggered")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    triggered: bool
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., triggered: _Optional[bool] = ...) -> None: ...

class SetSpindleAlarm(_message.Message):
    __slots__ = ("triggered",)
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    triggered: bool
    def __init__(self, triggered: _Optional[bool] = ...) -> None: ...

class GetSpindleAlarm(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class SpindleAlarmState(_message.Message):
    __slots__ = ("available", "triggered")
    AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    available: bool
    triggered: bool
    def __init__(self, available: _Optional[bool] = ..., triggered: _Optional[bool] = ...) -> None: ...

class SetRotaryAccessoryInstalled(_message.Message):
    __slots__ = ("installed",)
    INSTALLED_FIELD_NUMBER: _ClassVar[int]
    installed: bool
    def __init__(self, installed: _Optional[bool] = ...) -> None: ...

class SetMainButtonPressed(_message.Message):
    __slots__ = ("pressed",)
    PRESSED_FIELD_NUMBER: _ClassVar[int]
    pressed: bool
    def __init__(self, pressed: _Optional[bool] = ...) -> None: ...

class SetEStopPressed(_message.Message):
    __slots__ = ("pressed",)
    PRESSED_FIELD_NUMBER: _ClassVar[int]
    pressed: bool
    def __init__(self, pressed: _Optional[bool] = ...) -> None: ...

class GetFrontPanelState(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class FrontPanelState(_message.Message):
    __slots__ = ("main_button_pressed", "e_stop_pressed", "power_rails", "direct_rgb_available", "direct_rgb", "led_strip_available", "led_strip")
    MAIN_BUTTON_PRESSED_FIELD_NUMBER: _ClassVar[int]
    E_STOP_PRESSED_FIELD_NUMBER: _ClassVar[int]
    POWER_RAILS_FIELD_NUMBER: _ClassVar[int]
    DIRECT_RGB_AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    DIRECT_RGB_FIELD_NUMBER: _ClassVar[int]
    LED_STRIP_AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    LED_STRIP_FIELD_NUMBER: _ClassVar[int]
    main_button_pressed: bool
    e_stop_pressed: bool
    power_rails: PowerRailState
    direct_rgb_available: bool
    direct_rgb: RgbColor
    led_strip_available: bool
    led_strip: _containers.RepeatedCompositeFieldContainer[RgbColor]
    def __init__(self, main_button_pressed: _Optional[bool] = ..., e_stop_pressed: _Optional[bool] = ..., power_rails: _Optional[_Union[PowerRailState, _Mapping]] = ..., direct_rgb_available: _Optional[bool] = ..., direct_rgb: _Optional[_Union[RgbColor, _Mapping]] = ..., led_strip_available: _Optional[bool] = ..., led_strip: _Optional[_Iterable[_Union[RgbColor, _Mapping]]] = ...) -> None: ...

class PowerRailState(_message.Message):
    __slots__ = ("v12", "v24")
    V12_FIELD_NUMBER: _ClassVar[int]
    V24_FIELD_NUMBER: _ClassVar[int]
    v12: bool
    v24: bool
    def __init__(self, v12: _Optional[bool] = ..., v24: _Optional[bool] = ...) -> None: ...

class RgbColor(_message.Message):
    __slots__ = ("r", "g", "b")
    R_FIELD_NUMBER: _ClassVar[int]
    G_FIELD_NUMBER: _ClassVar[int]
    B_FIELD_NUMBER: _ClassVar[int]
    r: int
    g: int
    b: int
    def __init__(self, r: _Optional[int] = ..., g: _Optional[int] = ..., b: _Optional[int] = ...) -> None: ...

class SetAtcPocketTools(_message.Message):
    __slots__ = ("tools", "replace")
    TOOLS_FIELD_NUMBER: _ClassVar[int]
    REPLACE_FIELD_NUMBER: _ClassVar[int]
    tools: _containers.RepeatedCompositeFieldContainer[AtcPocketTool]
    replace: bool
    def __init__(self, tools: _Optional[_Iterable[_Union[AtcPocketTool, _Mapping]]] = ..., replace: _Optional[bool] = ...) -> None: ...

class SetSpindleTool(_message.Message):
    __slots__ = ("installed", "tool", "length_mm", "kind", "probe_tip_diameter_mm")
    INSTALLED_FIELD_NUMBER: _ClassVar[int]
    TOOL_FIELD_NUMBER: _ClassVar[int]
    LENGTH_MM_FIELD_NUMBER: _ClassVar[int]
    KIND_FIELD_NUMBER: _ClassVar[int]
    PROBE_TIP_DIAMETER_MM_FIELD_NUMBER: _ClassVar[int]
    installed: bool
    tool: int
    length_mm: float
    kind: ToolKind
    probe_tip_diameter_mm: float
    def __init__(self, installed: _Optional[bool] = ..., tool: _Optional[int] = ..., length_mm: _Optional[float] = ..., kind: _Optional[_Union[ToolKind, str]] = ..., probe_tip_diameter_mm: _Optional[float] = ...) -> None: ...

class SetProbeToolInstalled(_message.Message):
    __slots__ = ("installed",)
    INSTALLED_FIELD_NUMBER: _ClassVar[int]
    installed: bool
    def __init__(self, installed: _Optional[bool] = ...) -> None: ...

class SetToolSetterBox(_message.Message):
    __slots__ = ("bounds", "enabled")
    BOUNDS_FIELD_NUMBER: _ClassVar[int]
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    bounds: Box
    enabled: bool
    def __init__(self, bounds: _Optional[_Union[Box, _Mapping]] = ..., enabled: _Optional[bool] = ...) -> None: ...

class SetStockBox(_message.Message):
    __slots__ = ("bounds", "enabled")
    BOUNDS_FIELD_NUMBER: _ClassVar[int]
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    bounds: Box
    enabled: bool
    def __init__(self, bounds: _Optional[_Union[Box, _Mapping]] = ..., enabled: _Optional[bool] = ...) -> None: ...

class SetAdcInput(_message.Message):
    __slots__ = ("channel", "raw")
    CHANNEL_FIELD_NUMBER: _ClassVar[int]
    RAW_FIELD_NUMBER: _ClassVar[int]
    channel: int
    raw: int
    def __init__(self, channel: _Optional[int] = ..., raw: _Optional[int] = ...) -> None: ...

class GetAdcInput(_message.Message):
    __slots__ = ("channel",)
    CHANNEL_FIELD_NUMBER: _ClassVar[int]
    channel: int
    def __init__(self, channel: _Optional[int] = ...) -> None: ...

class SetTemperature(_message.Message):
    __slots__ = ("sensor", "celsius")
    SENSOR_FIELD_NUMBER: _ClassVar[int]
    CELSIUS_FIELD_NUMBER: _ClassVar[int]
    sensor: TemperatureSensor
    celsius: float
    def __init__(self, sensor: _Optional[_Union[TemperatureSensor, str]] = ..., celsius: _Optional[float] = ...) -> None: ...

class SetSwitchState(_message.Message):
    __slots__ = ("name", "on", "value", "has_value")
    NAME_FIELD_NUMBER: _ClassVar[int]
    ON_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    HAS_VALUE_FIELD_NUMBER: _ClassVar[int]
    name: SwitchName
    on: bool
    value: float
    has_value: bool
    def __init__(self, name: _Optional[_Union[SwitchName, str]] = ..., on: _Optional[bool] = ..., value: _Optional[float] = ..., has_value: _Optional[bool] = ...) -> None: ...

class GetSwitchState(_message.Message):
    __slots__ = ("name",)
    NAME_FIELD_NUMBER: _ClassVar[int]
    name: SwitchName
    def __init__(self, name: _Optional[_Union[SwitchName, str]] = ...) -> None: ...

class SwitchState(_message.Message):
    __slots__ = ("name", "available", "on", "value")
    NAME_FIELD_NUMBER: _ClassVar[int]
    AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    ON_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    name: SwitchName
    available: bool
    on: bool
    value: float
    def __init__(self, name: _Optional[_Union[SwitchName, str]] = ..., available: _Optional[bool] = ..., on: _Optional[bool] = ..., value: _Optional[float] = ...) -> None: ...

class GetLaserState(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class GetEepromBytes(_message.Message):
    __slots__ = ("offset", "length")
    OFFSET_FIELD_NUMBER: _ClassVar[int]
    LENGTH_FIELD_NUMBER: _ClassVar[int]
    offset: int
    length: int
    def __init__(self, offset: _Optional[int] = ..., length: _Optional[int] = ...) -> None: ...

class SetEepromBytes(_message.Message):
    __slots__ = ("offset", "data")
    OFFSET_FIELD_NUMBER: _ClassVar[int]
    DATA_FIELD_NUMBER: _ClassVar[int]
    offset: int
    data: bytes
    def __init__(self, offset: _Optional[int] = ..., data: _Optional[bytes] = ...) -> None: ...

class EepromBytes(_message.Message):
    __slots__ = ("offset", "data", "total_size")
    OFFSET_FIELD_NUMBER: _ClassVar[int]
    DATA_FIELD_NUMBER: _ClassVar[int]
    TOTAL_SIZE_FIELD_NUMBER: _ClassVar[int]
    offset: int
    data: bytes
    total_size: int
    def __init__(self, offset: _Optional[int] = ..., data: _Optional[bytes] = ..., total_size: _Optional[int] = ...) -> None: ...

class GetEepromFields(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class SetEepromFields(_message.Message):
    __slots__ = ("fields",)
    FIELDS_FIELD_NUMBER: _ClassVar[int]
    fields: _containers.RepeatedCompositeFieldContainer[EepromField]
    def __init__(self, fields: _Optional[_Iterable[_Union[EepromField, _Mapping]]] = ...) -> None: ...

class EepromFields(_message.Message):
    __slots__ = ("fields",)
    FIELDS_FIELD_NUMBER: _ClassVar[int]
    fields: _containers.RepeatedCompositeFieldContainer[EepromField]
    def __init__(self, fields: _Optional[_Iterable[_Union[EepromField, _Mapping]]] = ...) -> None: ...

class EepromField(_message.Message):
    __slots__ = ("name", "type", "number", "integer", "boolean")
    NAME_FIELD_NUMBER: _ClassVar[int]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    NUMBER_FIELD_NUMBER: _ClassVar[int]
    INTEGER_FIELD_NUMBER: _ClassVar[int]
    BOOLEAN_FIELD_NUMBER: _ClassVar[int]
    name: str
    type: EepromFieldType
    number: float
    integer: int
    boolean: bool
    def __init__(self, name: _Optional[str] = ..., type: _Optional[_Union[EepromFieldType, str]] = ..., number: _Optional[float] = ..., integer: _Optional[int] = ..., boolean: _Optional[bool] = ...) -> None: ...

class LaserState(_message.Message):
    __slots__ = ("available", "mode", "firing", "testing", "power_percent", "scale_percent")
    AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    MODE_FIELD_NUMBER: _ClassVar[int]
    FIRING_FIELD_NUMBER: _ClassVar[int]
    TESTING_FIELD_NUMBER: _ClassVar[int]
    POWER_PERCENT_FIELD_NUMBER: _ClassVar[int]
    SCALE_PERCENT_FIELD_NUMBER: _ClassVar[int]
    available: bool
    mode: bool
    firing: bool
    testing: bool
    power_percent: float
    scale_percent: float
    def __init__(self, available: _Optional[bool] = ..., mode: _Optional[bool] = ..., firing: _Optional[bool] = ..., testing: _Optional[bool] = ..., power_percent: _Optional[float] = ..., scale_percent: _Optional[float] = ...) -> None: ...

class GetMachineSnapshot(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class PhysicalIoSnapshot(_message.Message):
    __slots__ = ("probe_inputs", "cover", "front_panel", "motor_alarms", "spindle_alarm", "switches", "laser", "pwm_outputs")
    PROBE_INPUTS_FIELD_NUMBER: _ClassVar[int]
    COVER_FIELD_NUMBER: _ClassVar[int]
    FRONT_PANEL_FIELD_NUMBER: _ClassVar[int]
    MOTOR_ALARMS_FIELD_NUMBER: _ClassVar[int]
    SPINDLE_ALARM_FIELD_NUMBER: _ClassVar[int]
    SWITCHES_FIELD_NUMBER: _ClassVar[int]
    LASER_FIELD_NUMBER: _ClassVar[int]
    PWM_OUTPUTS_FIELD_NUMBER: _ClassVar[int]
    probe_inputs: ProbeInputs
    cover: CoverState
    front_panel: FrontPanelState
    motor_alarms: _containers.RepeatedCompositeFieldContainer[MotorAlarmState]
    spindle_alarm: SpindleAlarmState
    switches: _containers.RepeatedCompositeFieldContainer[SwitchState]
    laser: LaserState
    pwm_outputs: _containers.RepeatedCompositeFieldContainer[PwmOutput]
    def __init__(self, probe_inputs: _Optional[_Union[ProbeInputs, _Mapping]] = ..., cover: _Optional[_Union[CoverState, _Mapping]] = ..., front_panel: _Optional[_Union[FrontPanelState, _Mapping]] = ..., motor_alarms: _Optional[_Iterable[_Union[MotorAlarmState, _Mapping]]] = ..., spindle_alarm: _Optional[_Union[SpindleAlarmState, _Mapping]] = ..., switches: _Optional[_Iterable[_Union[SwitchState, _Mapping]]] = ..., laser: _Optional[_Union[LaserState, _Mapping]] = ..., pwm_outputs: _Optional[_Iterable[_Union[PwmOutput, _Mapping]]] = ...) -> None: ...

class StartInteractiveTransport(_message.Message):
    __slots__ = ("enable_uart", "tcp_ports", "log_traffic")
    ENABLE_UART_FIELD_NUMBER: _ClassVar[int]
    TCP_PORTS_FIELD_NUMBER: _ClassVar[int]
    LOG_TRAFFIC_FIELD_NUMBER: _ClassVar[int]
    enable_uart: bool
    tcp_ports: _containers.RepeatedScalarFieldContainer[int]
    log_traffic: bool
    def __init__(self, enable_uart: _Optional[bool] = ..., tcp_ports: _Optional[_Iterable[int]] = ..., log_traffic: _Optional[bool] = ...) -> None: ...

class StopInteractiveTransport(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class Status(_message.Message):
    __slots__ = ("time_us", "time_mode", "machine_model", "function_setting", "realtime_speed")
    TIME_US_FIELD_NUMBER: _ClassVar[int]
    TIME_MODE_FIELD_NUMBER: _ClassVar[int]
    MACHINE_MODEL_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_SETTING_FIELD_NUMBER: _ClassVar[int]
    REALTIME_SPEED_FIELD_NUMBER: _ClassVar[int]
    time_us: int
    time_mode: TimeMode
    machine_model: MachineModel
    function_setting: int
    realtime_speed: float
    def __init__(self, time_us: _Optional[int] = ..., time_mode: _Optional[_Union[TimeMode, str]] = ..., machine_model: _Optional[_Union[MachineModel, str]] = ..., function_setting: _Optional[int] = ..., realtime_speed: _Optional[float] = ...) -> None: ...

class GpioLevel(_message.Message):
    __slots__ = ("pin", "high")
    PIN_FIELD_NUMBER: _ClassVar[int]
    HIGH_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    high: bool
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ..., high: _Optional[bool] = ...) -> None: ...

class AttachedAxis(_message.Message):
    __slots__ = ("axis",)
    AXIS_FIELD_NUMBER: _ClassVar[int]
    axis: int
    def __init__(self, axis: _Optional[int] = ...) -> None: ...

class AxisPosition(_message.Message):
    __slots__ = ("axis", "steps")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    STEPS_FIELD_NUMBER: _ClassVar[int]
    axis: int
    steps: int
    def __init__(self, axis: _Optional[int] = ..., steps: _Optional[int] = ...) -> None: ...

class SerialData(_message.Message):
    __slots__ = ("data",)
    DATA_FIELD_NUMBER: _ClassVar[int]
    data: str
    def __init__(self, data: _Optional[str] = ...) -> None: ...

class RunResult(_message.Message):
    __slots__ = ("idle",)
    IDLE_FIELD_NUMBER: _ClassVar[int]
    idle: bool
    def __init__(self, idle: _Optional[bool] = ...) -> None: ...

class JogResult(_message.Message):
    __slots__ = ("idle", "serial_data")
    IDLE_FIELD_NUMBER: _ClassVar[int]
    SERIAL_DATA_FIELD_NUMBER: _ClassVar[int]
    idle: bool
    serial_data: str
    def __init__(self, idle: _Optional[bool] = ..., serial_data: _Optional[str] = ...) -> None: ...

class PwmOutput(_message.Message):
    __slots__ = ("pin", "configured", "duty", "period_us")
    PIN_FIELD_NUMBER: _ClassVar[int]
    CONFIGURED_FIELD_NUMBER: _ClassVar[int]
    DUTY_FIELD_NUMBER: _ClassVar[int]
    PERIOD_US_FIELD_NUMBER: _ClassVar[int]
    pin: PinAddress
    configured: bool
    duty: float
    period_us: float
    def __init__(self, pin: _Optional[_Union[PinAddress, _Mapping]] = ..., configured: _Optional[bool] = ..., duty: _Optional[float] = ..., period_us: _Optional[float] = ...) -> None: ...

class ProbeInputs(_message.Message):
    __slots__ = ("probe", "tool_setter")
    PROBE_FIELD_NUMBER: _ClassVar[int]
    TOOL_SETTER_FIELD_NUMBER: _ClassVar[int]
    probe: bool
    tool_setter: bool
    def __init__(self, probe: _Optional[bool] = ..., tool_setter: _Optional[bool] = ...) -> None: ...

class AdcInput(_message.Message):
    __slots__ = ("channel", "raw")
    CHANNEL_FIELD_NUMBER: _ClassVar[int]
    RAW_FIELD_NUMBER: _ClassVar[int]
    channel: int
    raw: int
    def __init__(self, channel: _Optional[int] = ..., raw: _Optional[int] = ...) -> None: ...

class MachineSnapshot(_message.Message):
    __slots__ = ("firmware_booted", "homed", "soft_endstop_enabled", "work_area", "axes", "atc", "physical_travel", "spindle", "tool_setter", "tool_setter_available")
    FIRMWARE_BOOTED_FIELD_NUMBER: _ClassVar[int]
    HOMED_FIELD_NUMBER: _ClassVar[int]
    SOFT_ENDSTOP_ENABLED_FIELD_NUMBER: _ClassVar[int]
    WORK_AREA_FIELD_NUMBER: _ClassVar[int]
    AXES_FIELD_NUMBER: _ClassVar[int]
    ATC_FIELD_NUMBER: _ClassVar[int]
    PHYSICAL_TRAVEL_FIELD_NUMBER: _ClassVar[int]
    SPINDLE_FIELD_NUMBER: _ClassVar[int]
    TOOL_SETTER_FIELD_NUMBER: _ClassVar[int]
    TOOL_SETTER_AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    firmware_booted: bool
    homed: bool
    soft_endstop_enabled: bool
    work_area: Box
    axes: _containers.RepeatedCompositeFieldContainer[AxisState]
    atc: AtcState
    physical_travel: Box
    spindle: SpindleState
    tool_setter: Box
    tool_setter_available: bool
    def __init__(self, firmware_booted: _Optional[bool] = ..., homed: _Optional[bool] = ..., soft_endstop_enabled: _Optional[bool] = ..., work_area: _Optional[_Union[Box, _Mapping]] = ..., axes: _Optional[_Iterable[_Union[AxisState, _Mapping]]] = ..., atc: _Optional[_Union[AtcState, _Mapping]] = ..., physical_travel: _Optional[_Union[Box, _Mapping]] = ..., spindle: _Optional[_Union[SpindleState, _Mapping]] = ..., tool_setter: _Optional[_Union[Box, _Mapping]] = ..., tool_setter_available: _Optional[bool] = ...) -> None: ...

class InteractiveTransport(_message.Message):
    __slots__ = ("uart_supported", "uart_path", "tcp_endpoints")
    UART_SUPPORTED_FIELD_NUMBER: _ClassVar[int]
    UART_PATH_FIELD_NUMBER: _ClassVar[int]
    TCP_ENDPOINTS_FIELD_NUMBER: _ClassVar[int]
    uart_supported: bool
    uart_path: str
    tcp_endpoints: _containers.RepeatedCompositeFieldContainer[TcpEndpoint]
    def __init__(self, uart_supported: _Optional[bool] = ..., uart_path: _Optional[str] = ..., tcp_endpoints: _Optional[_Iterable[_Union[TcpEndpoint, _Mapping]]] = ...) -> None: ...

class TcpEndpoint(_message.Message):
    __slots__ = ("host", "port")
    HOST_FIELD_NUMBER: _ClassVar[int]
    PORT_FIELD_NUMBER: _ClassVar[int]
    host: str
    port: int
    def __init__(self, host: _Optional[str] = ..., port: _Optional[int] = ...) -> None: ...

class MachineTelemetry(_message.Message):
    __slots__ = ("firmware_booted", "homed", "axes", "physical_travel", "spindle", "atc", "time_us")
    FIRMWARE_BOOTED_FIELD_NUMBER: _ClassVar[int]
    HOMED_FIELD_NUMBER: _ClassVar[int]
    AXES_FIELD_NUMBER: _ClassVar[int]
    PHYSICAL_TRAVEL_FIELD_NUMBER: _ClassVar[int]
    SPINDLE_FIELD_NUMBER: _ClassVar[int]
    ATC_FIELD_NUMBER: _ClassVar[int]
    TIME_US_FIELD_NUMBER: _ClassVar[int]
    firmware_booted: bool
    homed: bool
    axes: _containers.RepeatedCompositeFieldContainer[AxisState]
    physical_travel: Box
    spindle: SpindleState
    atc: AtcState
    time_us: int
    def __init__(self, firmware_booted: _Optional[bool] = ..., homed: _Optional[bool] = ..., axes: _Optional[_Iterable[_Union[AxisState, _Mapping]]] = ..., physical_travel: _Optional[_Union[Box, _Mapping]] = ..., spindle: _Optional[_Union[SpindleState, _Mapping]] = ..., atc: _Optional[_Union[AtcState, _Mapping]] = ..., time_us: _Optional[int] = ...) -> None: ...

class AxisState(_message.Message):
    __slots__ = ("axis", "physical_steps", "physical_mm", "machine_position", "endstop_triggered")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    PHYSICAL_STEPS_FIELD_NUMBER: _ClassVar[int]
    PHYSICAL_MM_FIELD_NUMBER: _ClassVar[int]
    MACHINE_POSITION_FIELD_NUMBER: _ClassVar[int]
    ENDSTOP_TRIGGERED_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    physical_steps: int
    physical_mm: float
    machine_position: float
    endstop_triggered: bool
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., physical_steps: _Optional[int] = ..., physical_mm: _Optional[float] = ..., machine_position: _Optional[float] = ..., endstop_triggered: _Optional[bool] = ...) -> None: ...

class SpindleState(_message.Message):
    __slots__ = ("spinning", "actual_rpm", "target_rpm", "max_rpm")
    SPINNING_FIELD_NUMBER: _ClassVar[int]
    ACTUAL_RPM_FIELD_NUMBER: _ClassVar[int]
    TARGET_RPM_FIELD_NUMBER: _ClassVar[int]
    MAX_RPM_FIELD_NUMBER: _ClassVar[int]
    spinning: bool
    actual_rpm: float
    target_rpm: float
    max_rpm: float
    def __init__(self, spinning: _Optional[bool] = ..., actual_rpm: _Optional[float] = ..., target_rpm: _Optional[float] = ..., max_rpm: _Optional[float] = ...) -> None: ...

class AtcState(_message.Message):
    __slots__ = ("available", "spindle", "pockets")
    AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    SPINDLE_FIELD_NUMBER: _ClassVar[int]
    POCKETS_FIELD_NUMBER: _ClassVar[int]
    available: bool
    spindle: ToolState
    pockets: _containers.RepeatedCompositeFieldContainer[AtcPocketTool]
    def __init__(self, available: _Optional[bool] = ..., spindle: _Optional[_Union[ToolState, _Mapping]] = ..., pockets: _Optional[_Iterable[_Union[AtcPocketTool, _Mapping]]] = ...) -> None: ...

class ToolState(_message.Message):
    __slots__ = ("active_tool", "target_tool", "tool_offset_mm", "cur_tool_mz", "ref_tool_mz", "target_collet_type", "length_mm", "kind", "probe_tip_diameter_mm")
    ACTIVE_TOOL_FIELD_NUMBER: _ClassVar[int]
    TARGET_TOOL_FIELD_NUMBER: _ClassVar[int]
    TOOL_OFFSET_MM_FIELD_NUMBER: _ClassVar[int]
    CUR_TOOL_MZ_FIELD_NUMBER: _ClassVar[int]
    REF_TOOL_MZ_FIELD_NUMBER: _ClassVar[int]
    TARGET_COLLET_TYPE_FIELD_NUMBER: _ClassVar[int]
    LENGTH_MM_FIELD_NUMBER: _ClassVar[int]
    KIND_FIELD_NUMBER: _ClassVar[int]
    PROBE_TIP_DIAMETER_MM_FIELD_NUMBER: _ClassVar[int]
    active_tool: int
    target_tool: int
    tool_offset_mm: float
    cur_tool_mz: float
    ref_tool_mz: float
    target_collet_type: int
    length_mm: float
    kind: ToolKind
    probe_tip_diameter_mm: float
    def __init__(self, active_tool: _Optional[int] = ..., target_tool: _Optional[int] = ..., tool_offset_mm: _Optional[float] = ..., cur_tool_mz: _Optional[float] = ..., ref_tool_mz: _Optional[float] = ..., target_collet_type: _Optional[int] = ..., length_mm: _Optional[float] = ..., kind: _Optional[_Union[ToolKind, str]] = ..., probe_tip_diameter_mm: _Optional[float] = ...) -> None: ...

class AtcPocketTool(_message.Message):
    __slots__ = ("pocket", "tool", "occupied", "length_mm", "x", "y", "z", "kind", "probe_tip_diameter_mm")
    POCKET_FIELD_NUMBER: _ClassVar[int]
    TOOL_FIELD_NUMBER: _ClassVar[int]
    OCCUPIED_FIELD_NUMBER: _ClassVar[int]
    LENGTH_MM_FIELD_NUMBER: _ClassVar[int]
    X_FIELD_NUMBER: _ClassVar[int]
    Y_FIELD_NUMBER: _ClassVar[int]
    Z_FIELD_NUMBER: _ClassVar[int]
    KIND_FIELD_NUMBER: _ClassVar[int]
    PROBE_TIP_DIAMETER_MM_FIELD_NUMBER: _ClassVar[int]
    pocket: int
    tool: int
    occupied: bool
    length_mm: float
    x: float
    y: float
    z: float
    kind: ToolKind
    probe_tip_diameter_mm: float
    def __init__(self, pocket: _Optional[int] = ..., tool: _Optional[int] = ..., occupied: _Optional[bool] = ..., length_mm: _Optional[float] = ..., x: _Optional[float] = ..., y: _Optional[float] = ..., z: _Optional[float] = ..., kind: _Optional[_Union[ToolKind, str]] = ..., probe_tip_diameter_mm: _Optional[float] = ...) -> None: ...

class Box(_message.Message):
    __slots__ = ("min_x", "min_y", "min_z", "max_x", "max_y", "max_z")
    MIN_X_FIELD_NUMBER: _ClassVar[int]
    MIN_Y_FIELD_NUMBER: _ClassVar[int]
    MIN_Z_FIELD_NUMBER: _ClassVar[int]
    MAX_X_FIELD_NUMBER: _ClassVar[int]
    MAX_Y_FIELD_NUMBER: _ClassVar[int]
    MAX_Z_FIELD_NUMBER: _ClassVar[int]
    min_x: float
    min_y: float
    min_z: float
    max_x: float
    max_y: float
    max_z: float
    def __init__(self, min_x: _Optional[float] = ..., min_y: _Optional[float] = ..., min_z: _Optional[float] = ..., max_x: _Optional[float] = ..., max_y: _Optional[float] = ..., max_z: _Optional[float] = ...) -> None: ...

class AxisDelta(_message.Message):
    __slots__ = ("axis", "distance")
    AXIS_FIELD_NUMBER: _ClassVar[int]
    DISTANCE_FIELD_NUMBER: _ClassVar[int]
    axis: Axis
    distance: float
    def __init__(self, axis: _Optional[_Union[Axis, str]] = ..., distance: _Optional[float] = ...) -> None: ...

class PinAddress(_message.Message):
    __slots__ = ("port", "pin")
    PORT_FIELD_NUMBER: _ClassVar[int]
    PIN_FIELD_NUMBER: _ClassVar[int]
    port: int
    pin: int
    def __init__(self, port: _Optional[int] = ..., pin: _Optional[int] = ...) -> None: ...
