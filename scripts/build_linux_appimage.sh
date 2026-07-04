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
APPIMAGE_ARCHITECTURE="$(linux_appimage_architecture "$MACHINE")"
ELF_MACHINE_PATTERN="$(linux_elf_machine_pattern "$MACHINE")"
FILE_MACHINE_PATTERN="$(linux_file_machine_pattern "$MACHINE")"
EXPECTED_ARCHITECTURE="${CARVERA_SIM_EXPECTED_ARTIFACT_ARCHITECTURE:-$ARTIFACT_ARCHITECTURE}"
if [[ "$EXPECTED_ARCHITECTURE" != "$ARTIFACT_ARCHITECTURE" ]]; then
  echo "error: expected $EXPECTED_ARCHITECTURE, but container is $ARTIFACT_ARCHITECTURE" >&2
  exit 2
fi

APP_NAME="Carvera Simulator"
BUILD_DIR="${CARVERA_SIM_PACKAGE_BUILD_DIR:-"$ROOT_DIR/build-linux-appimage-$ARTIFACT_ARCHITECTURE"}"
OUTPUT_DIR="${CARVERA_SIM_PACKAGE_OUTPUT_DIR:-"$ROOT_DIR/dist/linux"}"
PACKAGE_WORK_DIR="${CARVERA_SIM_PACKAGE_WORK_DIR:-"$BUILD_DIR/package"}"
PYINSTALLER_DIST="$PACKAGE_WORK_DIR/pyinstaller-dist"
PYINSTALLER_WORK="$PACKAGE_WORK_DIR/pyinstaller-work"
SPEC_DIR="$PACKAGE_WORK_DIR/spec"
APP_DIR="$PACKAGE_WORK_DIR/Carvera-Simulator.AppDir"
TOOLS_DIR="$BUILD_DIR/tools"
APPIMAGE_PATH="$OUTPUT_DIR/Carvera-Simulator-Linux-$ARTIFACT_ARCHITECTURE.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-$APPIMAGE_ARCHITECTURE.AppImage"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$APPIMAGE_ARCHITECTURE.AppImage"
APPIMAGETOOL_SHA256="$(linux_appimagetool_sha256 "$APPIMAGE_ARCHITECTURE")"
REQUIRED_QTWEBENGINE_LIBRARIES=(
  libasound.so.2
  liblcms2.so.2
  libminizip.so.1
  libopus.so.0
  libsnappy.so.1
  libwebp.so.7
  libwebpdemux.so.2
  libwebpmux.so.3
  libXfixes.so.3
)

for command in cmake curl file git ninja readelf sha256sum uv; do
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
mkdir -p "$PYINSTALLER_DIST" "$PYINSTALLER_WORK" "$SPEC_DIR" "$OUTPUT_DIR" "$TOOLS_DIR"
rm -f "$APPIMAGE_PATH"

uv run --project "$ROOT_DIR" --no-sync python -m PyInstaller \
  --name "$APP_NAME" \
  --onedir \
  --noconfirm \
  --clean \
  --hidden-import webview.platforms.qt \
  --paths "$ROOT_DIR" \
  --add-binary "$SIMULATOR_BINARY:bin" \
  --add-data "$ROOT_DIR/machine_models:machine_models" \
  --add-data "$ROOT_DIR/default_sdcard:default_sdcard" \
  --distpath "$PYINSTALLER_DIST" \
  --workpath "$PYINSTALLER_WORK" \
  --specpath "$SPEC_DIR" \
  "$ROOT_DIR/gui/desktop.py"

PYINSTALLER_APP="$PYINSTALLER_DIST/$APP_NAME"
if [[ ! -x "$PYINSTALLER_APP/$APP_NAME" ]]; then
  echo "error: PyInstaller did not create $PYINSTALLER_APP/$APP_NAME" >&2
  exit 1
fi

mkdir -p \
  "$APP_DIR/usr/bin" \
  "$APP_DIR/usr/lib/carvera-simulator" \
  "$APP_DIR/usr/share/applications" \
  "$APP_DIR/usr/share/icons/hicolor/scalable/apps"
cp -a --no-preserve=xattr "$PYINSTALLER_APP/." "$APP_DIR/usr/lib/carvera-simulator/"
for library in "${REQUIRED_QTWEBENGINE_LIBRARIES[@]}"; do
  if [[ -z "$(find "$APP_DIR/usr/lib/carvera-simulator" -name "$library" -print -quit)" ]]; then
    echo "error: packaged Qt WebEngine dependency is missing: $library" >&2
    exit 1
  fi
done
install -m 755 "$ROOT_DIR/packaging/linux/AppRun" "$APP_DIR/AppRun"
install -m 644 "$ROOT_DIR/packaging/linux/carvera-simulator.desktop" "$APP_DIR/carvera-simulator.desktop"
install -m 644 "$ROOT_DIR/packaging/linux/carvera-simulator.desktop" \
  "$APP_DIR/usr/share/applications/carvera-simulator.desktop"
install -m 644 "$ROOT_DIR/packaging/linux/carvera-simulator.svg" "$APP_DIR/carvera-simulator.svg"
install -m 644 "$ROOT_DIR/packaging/linux/carvera-simulator.svg" \
  "$APP_DIR/usr/share/icons/hicolor/scalable/apps/carvera-simulator.svg"
ln -s ../lib/carvera-simulator/Carvera\ Simulator "$APP_DIR/usr/bin/carvera-simulator"
ln -s carvera-simulator.svg "$APP_DIR/.DirIcon"

PACKAGED_SIMULATOR="$(find "$APP_DIR/usr/lib/carvera-simulator" -type f -name carvera_sim_stream_stdio -print -quit)"
if [[ -z "$PACKAGED_SIMULATOR" ]]; then
  echo "error: packaged simulator binary is missing" >&2
  exit 1
fi
if ! readelf -h "$PACKAGED_SIMULATOR" | grep -Fq "Machine:                           $ELF_MACHINE_PATTERN"; then
  echo "error: packaged simulator binary has the wrong architecture" >&2
  readelf -h "$PACKAGED_SIMULATOR" >&2
  exit 1
fi

if [[ ! -f "$APPIMAGETOOL" ]] || ! printf '%s  %s\n' "$APPIMAGETOOL_SHA256" "$APPIMAGETOOL" | sha256sum --check --status; then
  curl --fail --location --retry 3 --output "$APPIMAGETOOL" "$APPIMAGETOOL_URL"
fi
printf '%s  %s\n' "$APPIMAGETOOL_SHA256" "$APPIMAGETOOL" | sha256sum --check
chmod +x "$APPIMAGETOOL"

ARCH="$APPIMAGE_ARCHITECTURE" APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" \
  --no-appstream \
  --mksquashfs-opt=-no-xattrs \
  "$APP_DIR" \
  "$APPIMAGE_PATH"
chmod +x "$APPIMAGE_PATH"

if ! file "$APPIMAGE_PATH" | grep -Fq "$FILE_MACHINE_PATTERN"; then
  echo "error: AppImage has the wrong architecture" >&2
  file "$APPIMAGE_PATH" >&2
  exit 1
fi

printf 'Created %s\n' "$APPIMAGE_PATH"
