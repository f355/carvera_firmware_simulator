#!/usr/bin/env bash
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

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/linux_appimage_common.sh
source "$ROOT_DIR/scripts/linux_appimage_common.sh"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "error: Linux AppImages must be tested on Linux" >&2
  exit 2
fi

MACHINE="$(uname -m)"
ARTIFACT_ARCHITECTURE="$(linux_artifact_architecture "$MACHINE")"
FILE_MACHINE_PATTERN="$(linux_file_machine_pattern "$MACHINE")"
APPIMAGE_PATH="${1:-"$ROOT_DIR/dist/linux/Carvera-Simulator-Linux-$ARTIFACT_ARCHITECTURE.AppImage"}"
SMOKE_ROOT="${CARVERA_SIM_PACKAGE_SMOKE_DIR:-"$ROOT_DIR/build-linux-appimage-$ARTIFACT_ARCHITECTURE/smoke"}"
PORT="${CARVERA_SIM_PACKAGE_SMOKE_PORT:-18771}"
LOG_PATH="$SMOKE_ROOT/appimage.log"
PROCESS_PATTERN='[c]arvera-simulator|[C]arvera Backend|[c]arvera_sim_stream_stdio'

for command in curl file pgrep setsid xvfb-run; do
  command -v "$command" >/dev/null || { echo "error: $command is required" >&2; exit 2; }
done
if [[ ! -x "$APPIMAGE_PATH" ]]; then
  echo "error: AppImage is missing or not executable: $APPIMAGE_PATH" >&2
  exit 1
fi
if ! file "$APPIMAGE_PATH" | grep -Fq "$FILE_MACHINE_PATTERN"; then
  echo "error: AppImage has the wrong architecture" >&2
  file "$APPIMAGE_PATH" >&2
  exit 1
fi

rm -rf "$SMOKE_ROOT"
mkdir -p "$SMOKE_ROOT/home" "$SMOKE_ROOT/data"

APP_PID=""
cleanup() {
  if [[ -n "$APP_PID" ]] && kill -0 -- "-$APP_PID" 2>/dev/null; then
    kill -TERM -- "-$APP_PID" 2>/dev/null || true
  fi
  [[ -z "$APP_PID" ]] || wait "$APP_PID" 2>/dev/null || true
}
trap cleanup EXIT

HOME="$SMOKE_ROOT/home" \
XDG_DATA_HOME="$SMOKE_ROOT/data" \
APPIMAGE_EXTRACT_AND_RUN=1 \
setsid xvfb-run -a "$APPIMAGE_PATH" --host 127.0.0.1 --port "$PORT" --no-log-transport >"$LOG_PATH" 2>&1 &
APP_PID=$!

READY=0
for _ in $(seq 1 120); do
  if curl --fail --silent "http://127.0.0.1:$PORT/healthz" | grep -Fq '"ok":true'; then
    READY=1
    break
  fi
  if ! kill -0 "$APP_PID" 2>/dev/null; then
    break
  fi
  sleep 0.25
done
if (( ! READY )); then
  echo "error: packaged native GUI did not become healthy" >&2
  cat "$LOG_PATH" >&2
  exit 1
fi

WEBGL_READY=0
for _ in $(seq 1 120); do
  if grep -Fq 'WebGL2 renderer:' "$LOG_PATH"; then
    WEBGL_READY=1
    break
  fi
  if ! kill -0 "$APP_PID" 2>/dev/null; then
    break
  fi
  sleep 0.25
done
if (( ! WEBGL_READY )); then
  echo "error: packaged native GUI did not create a WebGL2 renderer" >&2
  cat "$LOG_PATH" >&2
  exit 1
fi
grep -F 'WebGL2 renderer:' "$LOG_PATH" | tail -1

cleanup
APP_PID=""
trap - EXIT

for _ in $(seq 1 20); do
  if ! pgrep -f "$PROCESS_PATTERN" >/dev/null; then
    break
  fi
  sleep 0.25
done

if pgrep -f "$PROCESS_PATTERN" >/dev/null; then
  echo "error: packaged application left child processes running" >&2
  pgrep -af "$PROCESS_PATTERN" >&2 || true
  exit 1
fi

printf 'Smoke-tested %s\n' "$APPIMAGE_PATH"
