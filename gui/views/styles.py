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


SIM_CSS = """
html, body, #app { height: 100%; margin: 0; overflow: hidden; }
body { background: #f6f7f9; color: #1f2933; }
.nicegui-content { padding: 0 !important; }
.sim-page { width: 100vw; height: 100vh; display: grid; grid-template-rows: auto minmax(0, 1fr); overflow: hidden; }
.sim-toolbar {
  display: flex; align-items: center; gap: 16px; flex-wrap: wrap;
  box-sizing: border-box; width: 100vw; padding: 10px 14px; border-bottom: 1px solid #d7dde5; background: #ffffff;
}
.sim-title { font-size: 17px; font-weight: 650; margin-right: 6px; }
.header-controls {
  flex: 1 1 auto; min-width: 0; display: flex; align-items: center; justify-content: flex-end; gap: 10px;
}
.primary-controls { display: flex; align-items: center; gap: 10px; padding-left: 12px; border-left: 1px solid #e2e8f0; }
.primary-controls .q-btn { min-height: 34px; }
.primary-controls .q-toggle { white-space: nowrap; }
.main-button-led { transition: background-color 120ms ease, border-color 120ms ease, box-shadow 120ms ease; }
.transport-strip { display: flex; gap: 12px; flex-wrap: wrap; }
.transport-readout {
  min-width: 190px; max-width: 360px; font-size: 12px; color: #4b5563;
  font-variant-numeric: tabular-nums; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.axis-strip { display: flex; gap: 8px; flex: 0 1 auto; min-width: 0; flex-wrap: nowrap; }
.axis-readout {
  width: 96px; padding: 6px 8px; border: 1px solid #d7dde5; border-radius: 6px;
  background: #fbfcfd; font-variant-numeric: tabular-nums;
}
.axis-readout .axis-name { font-size: 11px; color: #657282; text-transform: uppercase; }
.axis-readout .axis-value { font-size: 15px; font-weight: 650; }
.main-splitter { width: 100vw; height: 100%; min-height: 0; overflow: hidden; }
.main-splitter .q-splitter__before, .main-splitter .q-splitter__after { min-height: 0; overflow: hidden; }
.main-splitter .q-splitter__separator { background: #cbd5e1; width: 6px; }
.scene-pane { width: 100%; height: 100%; min-height: 0; overflow: hidden; }
.machine-scene { flex: 1 1 auto; width: 100%; height: 100%; min-height: 0; min-width: 0; }
.machine-scene canvas { display: block; width: 100%; height: 100%; }
.side-panel {
  width: 100%; height: 100%; min-height: 0; overflow: hidden;
  background: #ffffff; display: grid; grid-template-rows: auto 1fr;
}
.side-tabs { padding: 6px 8px 0; border-bottom: 1px solid #e2e8f0; }
.side-tabs .q-tab { padding: 0 10px; min-height: 44px; }
.side-tabs .q-tab__label { font-size: 12px; font-weight: 650; }
.side-panels { min-height: 0; overflow-y: auto; }
.q-tab-panel { padding: 12px 14px; }
.panel-section { border-bottom: 1px solid #e5eaf0; padding: 0 0 12px; margin-bottom: 12px; }
.panel-section:last-child { border-bottom: 0; margin-bottom: 0; }
.section-title { font-size: 12px; font-weight: 700; color: #334155; text-transform: uppercase; }
.section-subtle { font-size: 12px; color: #64748b; }
.metrics-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px; margin-top: 8px; }
.metric {
  min-width: 0; border-left: 3px solid #d7dde5; padding: 5px 8px; background: #fbfcfd;
}
.metric-name { font-size: 11px; color: #64748b; }
.metric-value {
  min-height: 20px; font-size: 14px; font-weight: 650; font-variant-numeric: tabular-nums;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.led-strip-row { display: flex; gap: 4px; margin-top: 6px; }
.led-segment {
  width: 24px; height: 12px; border: 1px solid #94a3b8; background: #cbd5e1;
  box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.35);
  transition: background-color 120ms ease;
}
.badge-on, .badge-off, .badge-warn {
  display: inline-flex; align-items: center; gap: 5px; min-width: 52px;
  border-radius: 0; padding: 1px 0; font-size: 11px; font-weight: 700; background: transparent;
}
.badge-on::before, .badge-off::before, .badge-warn::before {
  content: ""; width: 8px; height: 8px; border-radius: 999px; display: inline-block;
}
.badge-on { color: #166534; }
.badge-off { color: #475569; }
.badge-warn { color: #991b1b; }
.badge-on::before { background: #22c55e; box-shadow: 0 0 0 2px #dcfce7; }
.badge-off::before { background: #94a3b8; box-shadow: 0 0 0 2px #e5e7eb; }
.badge-warn::before { background: #ef4444; box-shadow: 0 0 0 2px #fee2e2; }
.axis-table, .tool-table, .pin-table, .pwm-table, .switch-table {
  display: grid; gap: 6px; margin-top: 8px; font-size: 12px;
}
.axis-table { grid-template-columns: 44px 1fr 1fr 64px; }
.tool-table-wrap { overflow-x: auto; margin-top: 8px; }
.tool-table {
  min-width: 620px;
  grid-template-columns: 72px 58px 58px 78px 62px 82px 60px;
  align-items: center;
}
.tool-status-table { grid-template-columns: 32px 58px minmax(92px, 1fr) 86px; }
.pin-table { grid-template-columns: 1fr 54px 72px; }
.pwm-table { grid-template-columns: 1fr 72px 86px; }
.switch-table { grid-template-columns: 1fr 76px 66px; align-items: center; }
.table-head { color: #64748b; font-size: 11px; font-weight: 700; text-transform: uppercase; }
.table-cell {
  min-height: 34px; display: flex; align-items: center; min-width: 0;
  font-variant-numeric: tabular-nums;
}
.table-cell .q-field { width: 100%; }
.plain-number input::-webkit-outer-spin-button, .plain-number input::-webkit-inner-spin-button {
  -webkit-appearance: none; margin: 0;
}
.plain-number input[type="number"] { appearance: textfield; -moz-appearance: textfield; }
.coord-text { color: #64748b; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.tool-presence { color: #475569; font-weight: 650; }
.control-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px 10px; margin-top: 8px; }
.button-row { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 10px; }
.input-grid { display: grid; grid-template-columns: 1fr 72px; gap: 6px 10px; margin-top: 8px; }
.contact-grid { display: grid; grid-template-columns: 1fr 72px 1fr 72px; gap: 6px 10px; margin-top: 8px; }
.appearance-grid { display: grid; grid-template-columns: minmax(0, 1fr); gap: 10px; margin-top: 8px; }
.appearance-control { min-width: 0; }
.appearance-control .q-slider { padding: 0 4px; }
.custom-gpio { display: grid; grid-template-columns: 70px 70px 1fr; gap: 8px; align-items: end; margin-top: 8px; }
.box-grid { display: grid; grid-template-columns: 70px 70px 70px 70px 70px 70px; gap: 6px; margin-top: 8px; }
.temperature-drive { display: grid; grid-template-columns: 132px 94px 1fr; gap: 8px; align-items: end; margin-top: 8px; }
.compact-field .q-field__control { min-height: 32px; height: 32px; }
.compact-actions { display: flex; gap: 4px; flex-wrap: nowrap; }
.eeprom-fields {
  display: grid; gap: 12px; margin-top: 8px;
}
.eeprom-field-group {
  display: grid; gap: 6px; padding-bottom: 8px; border-bottom: 1px solid #eef2f7;
}
.eeprom-field-group:last-child { border-bottom: 0; }
.eeprom-group-title {
  font-size: 12px; font-weight: 700; color: #334155;
}
.eeprom-fields-grid {
  display: grid; grid-template-columns: minmax(120px, 1fr) minmax(92px, 136px); gap: 5px 10px;
  align-items: center; font-size: 12px;
}
.eeprom-field-name {
  min-height: 32px; color: #475569; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.comm-toolbar { display: flex; align-items: center; gap: 10px; }
.comm-log {
  display: block; width: 100%; box-sizing: border-box;
  height: calc(100vh - 176px); overflow: auto; background: #0f172a; color: #dbeafe;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; font-size: 12px;
  padding: 8px; border: 1px solid #1e293b;
}
.comm-row { display: grid; grid-template-columns: 176px 44px 30px 1fr; gap: 8px; padding: 2px 0; }
.comm-time { color: #94a3b8; }
.comm-channel { color: #93c5fd; }
.comm-direction { font-weight: 700; }
.comm-rx .comm-direction { color: #fbbf24; }
.comm-tx .comm-direction { color: #34d399; }
.comm-payload { white-space: pre-wrap; overflow-wrap: anywhere; color: #e2e8f0; }
"""
