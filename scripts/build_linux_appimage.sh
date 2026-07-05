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
  echo "error: Linux AppImages must be built on Linux" >&2
  exit 2
fi

MACHINE="$(uname -m)"
ARTIFACT_ARCHITECTURE="$(linux_artifact_architecture "$MACHINE")"
ELECTRON_ARCHITECTURE="$(linux_electron_architecture "$MACHINE")"
ELF_MACHINE_PATTERN="$(linux_elf_machine_pattern "$MACHINE")"
FILE_MACHINE_PATTERN="$(linux_file_machine_pattern "$MACHINE")"
EXPECTED_ARCHITECTURE="${CARVERA_SIM_EXPECTED_ARTIFACT_ARCHITECTURE:-$ARTIFACT_ARCHITECTURE}"
if [[ "$EXPECTED_ARCHITECTURE" != "$ARTIFACT_ARCHITECTURE" ]]; then
  echo "error: expected $EXPECTED_ARCHITECTURE, but container is $ARTIFACT_ARCHITECTURE" >&2
  exit 2
fi

BACKEND_NAME="Carvera Backend"
BUILD_DIR="${CARVERA_SIM_PACKAGE_BUILD_DIR:-"$ROOT_DIR/build-linux-appimage-$ARTIFACT_ARCHITECTURE"}"
OUTPUT_DIR="${CARVERA_SIM_PACKAGE_OUTPUT_DIR:-"$ROOT_DIR/dist/linux"}"
PACKAGE_WORK_DIR="${CARVERA_SIM_PACKAGE_WORK_DIR:-"$BUILD_DIR/package"}"
PYINSTALLER_DIST="$PACKAGE_WORK_DIR/pyinstaller-dist"
PYINSTALLER_WORK="$PACKAGE_WORK_DIR/pyinstaller-work"
SPEC_DIR="$PACKAGE_WORK_DIR/spec"
ELECTRON_APP_DIR="$PACKAGE_WORK_DIR/electron"
ELECTRON_OUTPUT_DIR="$PACKAGE_WORK_DIR/electron-dist"
APPIMAGE_PATH="$OUTPUT_DIR/Carvera-Simulator-Linux-$ARTIFACT_ARCHITECTURE.AppImage"

for command in cmake file git ninja readelf rsvg-convert uv; do
  command -v "$command" >/dev/null || { echo "error: $command is required" >&2; exit 2; }
done

FIRMWARE_ROOT="$("$ROOT_DIR/scripts/ensure_firmware_checkout.sh")"
JOBS="$(nproc)"
export UV_PROJECT_ENVIRONMENT="$BUILD_DIR/python-environment"
export UV_LINK_MODE=copy

uv sync --project "$ROOT_DIR" --locked --no-dev --group package-linux --python 3.13

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCARVERA_FIRMWARE_ROOT="$FIRMWARE_ROOT"
cmake --build "$BUILD_DIR" --target carvera_sim_stream_stdio --parallel "$JOBS"

SIMULATOR_BINARY="$BUILD_DIR/carvera_sim_stream_stdio"
if ! readelf -h "$SIMULATOR_BINARY" | grep -Fq "Machine:                           $ELF_MACHINE_PATTERN"; then
  echo "error: simulator binary has the wrong architecture" >&2
  readelf -h "$SIMULATOR_BINARY" >&2
  exit 1
fi

rm -rf "$PACKAGE_WORK_DIR"
mkdir -p "$PYINSTALLER_DIST" "$PYINSTALLER_WORK" "$SPEC_DIR" "$OUTPUT_DIR"
rm -f "$APPIMAGE_PATH"

uv run --project "$ROOT_DIR" --no-sync python -m PyInstaller \
  --name "$BACKEND_NAME" \
  --onedir \
  --noconfirm \
  --clean \
  --paths "$ROOT_DIR" \
  --add-binary "$SIMULATOR_BINARY:bin" \
  --add-data "$ROOT_DIR/machine_models:machine_models" \
  --add-data "$ROOT_DIR/default_sdcard:default_sdcard" \
  --distpath "$PYINSTALLER_DIST" \
  --workpath "$PYINSTALLER_WORK" \
  --specpath "$SPEC_DIR" \
  "$ROOT_DIR/gui/app.py"

PYINSTALLER_APP="$PYINSTALLER_DIST/$BACKEND_NAME"
if [[ ! -x "$PYINSTALLER_APP/$BACKEND_NAME" ]]; then
  echo "error: PyInstaller did not create $PYINSTALLER_APP/$BACKEND_NAME" >&2
  exit 1
fi

mkdir -p "$ELECTRON_APP_DIR/resources/backend"
cp -a "$ROOT_DIR/packaging/linux/electron/." "$ELECTRON_APP_DIR/"
if [[ ! -d /opt/carvera-electron/node_modules ]]; then
  echo "error: Electron packaging dependencies are missing from the builder image" >&2
  exit 1
fi
cp -a /opt/carvera-electron/node_modules "$ELECTRON_APP_DIR/"
cp -a "$PYINSTALLER_APP" "$ELECTRON_APP_DIR/resources/backend/"
rsvg-convert --width 512 --height 512 \
  --output "$ELECTRON_APP_DIR/icon.png" \
  "$ROOT_DIR/packaging/linux/carvera-simulator.svg"

PACKAGED_SIMULATOR="$(find "$ELECTRON_APP_DIR/resources/backend" -type f -name carvera_sim_stream_stdio -print -quit)"
if [[ -z "$PACKAGED_SIMULATOR" ]]; then
  echo "error: packaged simulator binary is missing" >&2
  exit 1
fi
if ! readelf -h "$PACKAGED_SIMULATOR" | grep -Fq "Machine:                           $ELF_MACHINE_PATTERN"; then
  echo "error: packaged simulator binary has the wrong architecture" >&2
  readelf -h "$PACKAGED_SIMULATOR" >&2
  exit 1
fi

(
  cd "$ELECTRON_APP_DIR"
  ./node_modules/.bin/electron-builder \
    --linux AppImage \
    --"$ELECTRON_ARCHITECTURE" \
    --publish never \
    --config.directories.output="$ELECTRON_OUTPUT_DIR"
)

BUILT_APPIMAGE="$(find "$ELECTRON_OUTPUT_DIR" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"
if [[ -z "$BUILT_APPIMAGE" ]]; then
  echo "error: electron-builder did not create an AppImage" >&2
  exit 1
fi
mv "$BUILT_APPIMAGE" "$APPIMAGE_PATH"
chmod 755 "$APPIMAGE_PATH"

if ! file "$APPIMAGE_PATH" | grep -Fq "$FILE_MACHINE_PATTERN"; then
  echo "error: AppImage has the wrong architecture" >&2
  file "$APPIMAGE_PATH" >&2
  exit 1
fi

printf 'Created %s\n' "$APPIMAGE_PATH"
