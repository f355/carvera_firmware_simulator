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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${CARVERA_SIM_BUILD_DIR:-"$ROOT_DIR/build"}"
FIRMWARE_ROOT="${CARVERA_FIRMWARE_ROOT:-}"
FIRMWARE_DIR="${CARVERA_SIM_FIRMWARE_DIR:-"$ROOT_DIR/firmware/Carvera_Community_Firmware"}"
BUILD_JOBS="${CARVERA_SIM_BUILD_JOBS:-}"
HOST="${CARVERA_SIM_GUI_HOST:-127.0.0.1}"
PORT="${CARVERA_SIM_GUI_PORT:-8080}"
WIFI_PORT="${CARVERA_SIM_WIFI_PORT:-2222}"
MODEL="${CARVERA_SIM_MODEL:-c1}"
OPEN_BROWSER=1
GUI_ARGS=()

usage() {
  cat <<EOF
Usage: ./run_gui.sh [options] [-- extra gui args]

Builds the simulator, syncs the Python uv environment, starts the NiceGUI
frontend, and opens it in your browser.

Options:
  --firmware-root PATH  Existing firmware checkout root. Defaults to
                        CARVERA_FIRMWARE_ROOT. When omitted, the script clones
                        the pinned compatible firmware revision.
  --firmware-dir PATH   Managed firmware checkout directory. Defaults to
                        ./firmware/Carvera_Community_Firmware.
  --build-dir PATH      CMake build directory. Defaults to ./build.
  --host HOST           GUI bind host. Defaults to 127.0.0.1.
  --port PORT           GUI port. Defaults to 8080.
  --wifi-port PORT      Fake WiFi controller port. Defaults to 2222.
  --model c1|ca1        Initial machine model. Defaults to c1.
  --machine-model PATH  Optional GLB/GLTF/STL shell asset for the 3D viewport.
                        CA1 uses ./machine_models/carvera_air_ca1.glb by default
                        when the file exists. Other machine-model flags are
                        forwarded to the GUI; see -- --help.
  --ahb-bytes N         Cap simulated LPC AHB pool (e.g. 17544 for strict LPC).
  --ahb-unlimited       Disable the capped AHB pool.
  --stack-limit-bytes N Optional firmware native stack watermark.
  --lpc-heap            Enable LPC main-SRAM/_sbrk model (heap↔config-cache repro).
  --no-open             Do not open a browser.
  --no-log-transport    Do not dump UART/WiFi traffic to this terminal.
  -h, --help            Show this help.

Environment alternatives:
  CARVERA_FIRMWARE_ROOT, CARVERA_SIM_FIRMWARE_DIR, CARVERA_SIM_BUILD_DIR, CARVERA_SIM_GUI_HOST,
  CARVERA_SIM_GUI_PORT, CARVERA_SIM_WIFI_PORT, CARVERA_SIM_MODEL, CARVERA_SIM_BUILD_JOBS,
  CARVERA_SIM_AHB_BYTES, CARVERA_SIM_AHB_UNLIMITED, CARVERA_SIM_STACK_LIMIT_BYTES,
  CARVERA_SIM_LPC_HEAP
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --firmware-root)
      FIRMWARE_ROOT="$2"
      shift 2
      ;;
    --firmware-dir)
      FIRMWARE_DIR="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --host)
      HOST="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --wifi-port)
      WIFI_PORT="$2"
      shift 2
      ;;
    --model)
      MODEL="$2"
      shift 2
      ;;
    --no-open)
      OPEN_BROWSER=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      GUI_ARGS+=("$@")
      break
      ;;
    *)
      GUI_ARGS+=("$1")
      shift
      ;;
  esac
done

FIRMWARE_HELPER="$ROOT_DIR/scripts/ensure_firmware_checkout.sh"
if [[ ! -x "$FIRMWARE_HELPER" ]]; then
  echo "error: expected firmware checkout helper at $FIRMWARE_HELPER" >&2
  exit 2
fi

if [[ -n "$FIRMWARE_ROOT" ]]; then
  FIRMWARE_ROOT="$("$FIRMWARE_HELPER" --firmware-root "$FIRMWARE_ROOT")"
else
  FIRMWARE_ROOT="$("$FIRMWARE_HELPER" --firmware-dir "$FIRMWARE_DIR")"
fi

if [[ ! -f "$FIRMWARE_ROOT/src/main.cpp" ]]; then
  echo "error: firmware checkout is missing src/main.cpp: $FIRMWARE_ROOT" >&2
  exit 2
fi

if [[ "$MODEL" != "c1" && "$MODEL" != "ca1" ]]; then
  echo "error: --model must be c1 or ca1" >&2
  exit 2
fi

command -v cmake >/dev/null || { echo "error: cmake is required" >&2; exit 2; }
command -v uv >/dev/null || { echo "error: uv is required" >&2; exit 2; }

SIMULATOR_BIN="$BUILD_DIR/carvera_sim_stream_stdio"
URL="http://$HOST:$PORT"

open_url() {
  case "$(uname -s)" in
    Darwin)
      open "$URL"
      ;;
    Linux)
      if command -v xdg-open >/dev/null; then
        xdg-open "$URL" >/dev/null 2>&1 &
      else
        echo "GUI ready at $URL"
      fi
      ;;
    MINGW*|MSYS*|CYGWIN*)
      cmd.exe /c start "" "$URL" >/dev/null 2>&1
      ;;
    *)
      echo "GUI ready at $URL"
      ;;
  esac
}

open_when_ready() {
  if command -v curl >/dev/null; then
    for _ in $(seq 1 100); do
      if curl -fsS "$URL/healthz" >/dev/null 2>&1; then
        open_url
        return
      fi
      sleep 0.1
    done
  else
    sleep 2
    open_url
    return
  fi

  echo "GUI did not answer at $URL; leaving server running for inspection." >&2
}

detect_build_jobs() {
  if [[ -n "$BUILD_JOBS" ]]; then
    echo "$BUILD_JOBS"
    return
  fi
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
    return
  fi
  getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
}

echo "Configuring simulator build..."
CMAKE_ARGS=(-S "$ROOT_DIR" -B "$BUILD_DIR" -DCARVERA_FIRMWARE_ROOT="$FIRMWARE_ROOT")
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" && -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then
  CMAKE_ARGS+=(-G Ninja)
fi
cmake "${CMAKE_ARGS[@]}"

echo "Building simulator runtime..."
cmake --build "$BUILD_DIR" --target carvera_sim_stream_stdio --parallel "$(detect_build_jobs)"

echo "Syncing Python environment..."
cd "$ROOT_DIR"
uv sync
uv run python -m gui.protocol.proto_codegen

if [[ ! -x "$SIMULATOR_BIN" ]]; then
  echo "error: expected simulator binary at $SIMULATOR_BIN" >&2
  exit 1
fi

if [[ "$OPEN_BROWSER" == "1" ]]; then
  open_when_ready &
fi

echo "Starting GUI at $URL"
exec uv run python -m gui.app \
  --host "$HOST" \
  --port "$PORT" \
  --wifi-port "$WIFI_PORT" \
  --firmware-root "$FIRMWARE_ROOT" \
  --simulator "$SIMULATOR_BIN" \
  --model "$MODEL" \
  "${GUI_ARGS[@]}"
