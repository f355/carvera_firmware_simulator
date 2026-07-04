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
APP_NAME="Carvera Simulator"
BUNDLE_IDENTIFIER="org.f355.carvera-simulator"
BUILD_DIR="${CARVERA_SIM_PACKAGE_BUILD_DIR:-"$ROOT_DIR/build-macos-app"}"
OUTPUT_DIR="${CARVERA_SIM_PACKAGE_OUTPUT_DIR:-"$ROOT_DIR/dist/macos"}"
PACKAGE_WORK_DIR="${CARVERA_SIM_PACKAGE_WORK_DIR:-"$BUILD_DIR/package"}"
PYINSTALLER_DIST="$PACKAGE_WORK_DIR/dist"
PYINSTALLER_WORK="$PACKAGE_WORK_DIR/work"
SPEC_DIR="$PACKAGE_WORK_DIR/spec"
DMG_STAGE="$PACKAGE_WORK_DIR/dmg-root"
DMG_MOUNT="$PACKAGE_WORK_DIR/dmg-mount"
APP_PATH="$PYINSTALLER_DIST/$APP_NAME.app"
DMG_PATH="$OUTPUT_DIR/Carvera-Simulator-macOS-arm64.dmg"

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
  echo "error: the macOS package must be built on an ARM64 Mac" >&2
  exit 2
fi

for command in cmake hdiutil lipo ninja uv; do
  command -v "$command" >/dev/null || { echo "error: $command is required" >&2; exit 2; }
done

FIRMWARE_ROOT="$("$ROOT_DIR/scripts/ensure_firmware_checkout.sh")"
JOBS="$(sysctl -n hw.ncpu)"

uv sync --project "$ROOT_DIR" --group package

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCARVERA_FIRMWARE_ROOT="$FIRMWARE_ROOT"
cmake --build "$BUILD_DIR" --target carvera_sim_stream_stdio --parallel "$JOBS"

SIMULATOR_BINARY="$BUILD_DIR/carvera_sim_stream_stdio"
if [[ "$(lipo -archs "$SIMULATOR_BINARY")" != "arm64" ]]; then
  echo "error: simulator binary is not ARM64: $(lipo -archs "$SIMULATOR_BINARY")" >&2
  exit 1
fi

rm -rf "$PACKAGE_WORK_DIR" "$OUTPUT_DIR"
mkdir -p "$PYINSTALLER_DIST" "$PYINSTALLER_WORK" "$SPEC_DIR" "$OUTPUT_DIR"

uv run --project "$ROOT_DIR" python -m PyInstaller \
  --name "$APP_NAME" \
  --windowed \
  --onedir \
  --noconfirm \
  --clean \
  --target-architecture arm64 \
  --osx-bundle-identifier "$BUNDLE_IDENTIFIER" \
  --paths "$ROOT_DIR" \
  --add-binary "$SIMULATOR_BINARY:bin" \
  --add-data "$ROOT_DIR/machine_models:machine_models" \
  --add-data "$ROOT_DIR/default_sdcard:default_sdcard" \
  --distpath "$PYINSTALLER_DIST" \
  --workpath "$PYINSTALLER_WORK" \
  --specpath "$SPEC_DIR" \
  "$ROOT_DIR/gui/desktop.py"

if [[ ! -d "$APP_PATH" ]]; then
  echo "error: PyInstaller did not create $APP_PATH" >&2
  exit 1
fi

PACKAGED_SIMULATOR="$(find "$APP_PATH" -type f -name carvera_sim_stream_stdio -print -quit)"
if [[ -z "$PACKAGED_SIMULATOR" ]]; then
  echo "error: packaged simulator binary is missing" >&2
  exit 1
fi
if [[ "$(lipo -archs "$PACKAGED_SIMULATOR")" != "arm64" ]]; then
  echo "error: packaged simulator binary is not ARM64" >&2
  exit 1
fi
if otool -L "$PACKAGED_SIMULATOR" | grep -q '/opt/homebrew'; then
  echo "error: packaged simulator still references Homebrew libraries" >&2
  otool -L "$PACKAGED_SIMULATOR" >&2
  exit 1
fi

mkdir -p "$DMG_STAGE"
ditto "$APP_PATH" "$DMG_STAGE/$APP_NAME.app"
ln -s /Applications "$DMG_STAGE/Applications"
hdiutil create \
  -volname "$APP_NAME" \
  -srcfolder "$DMG_STAGE" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

DMG_MOUNTED=0
cleanup_mount() {
  if (( DMG_MOUNTED )); then
    hdiutil detach "$DMG_MOUNT" >/dev/null || true
  fi
}
trap cleanup_mount EXIT

mkdir -p "$DMG_MOUNT"
hdiutil attach -readonly -nobrowse -mountpoint "$DMG_MOUNT" "$DMG_PATH" >/dev/null
DMG_MOUNTED=1
if [[ ! -d "$DMG_MOUNT/$APP_NAME.app" || ! -L "$DMG_MOUNT/Applications" ]]; then
  echo "error: DMG does not contain the application and Applications shortcut" >&2
  exit 1
fi
hdiutil detach "$DMG_MOUNT" >/dev/null
DMG_MOUNTED=0
trap - EXIT

printf 'Created %s\n' "$DMG_PATH"
