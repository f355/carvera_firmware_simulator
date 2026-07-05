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

if [[ -n "${CARVERA_SIM_CONTAINER_ENGINE:-}" ]]; then
  CONTAINER_ENGINE="$CARVERA_SIM_CONTAINER_ENGINE"
elif command -v podman >/dev/null && podman info >/dev/null 2>&1; then
  CONTAINER_ENGINE=podman
elif command -v docker >/dev/null && docker info >/dev/null 2>&1; then
  CONTAINER_ENGINE=docker
else
  echo "error: Podman or Docker is required" >&2
  exit 2
fi
command -v "$CONTAINER_ENGINE" >/dev/null || { echo "error: $CONTAINER_ENGINE is not installed" >&2; exit 2; }

HOST_MACHINE="$(uname -m)"
ARTIFACT_ARCHITECTURE="$(linux_artifact_architecture "$HOST_MACHINE")"
PLATFORM="linux/$ARTIFACT_ARCHITECTURE"
EXPECTED_ARCHITECTURE="${CARVERA_SIM_EXPECTED_ARTIFACT_ARCHITECTURE:-$ARTIFACT_ARCHITECTURE}"
IMAGE_NAME="carvera-simulator-appimage-builder:ubuntu-24.04"
SMOKE_IMAGE_NAME="carvera-simulator-appimage-smoke:ubuntu-24.04"

"$CONTAINER_ENGINE" build \
  --platform "$PLATFORM" \
  --file "$ROOT_DIR/packaging/linux/Containerfile" \
  --target builder \
  --tag "$IMAGE_NAME" \
  "$ROOT_DIR"

"$CONTAINER_ENGINE" build \
  --platform "$PLATFORM" \
  --file "$ROOT_DIR/packaging/linux/Containerfile" \
  --target smoke \
  --tag "$SMOKE_IMAGE_NAME" \
  "$ROOT_DIR"

"$CONTAINER_ENGINE" run --rm \
  --platform "$PLATFORM" \
  --env CARVERA_SIM_EXPECTED_ARTIFACT_ARCHITECTURE="$EXPECTED_ARCHITECTURE" \
  --env CARVERA_SIM_PACKAGE_WORK_DIR=/tmp/carvera-simulator-package \
  --volume "$ROOT_DIR:/work" \
  --workdir /work \
  "$IMAGE_NAME" \
  ./scripts/build_linux_appimage.sh

APPIMAGE_PATH="$ROOT_DIR/dist/linux/Carvera-Simulator-Linux-$ARTIFACT_ARCHITECTURE.AppImage"
if [[ ! -f "$APPIMAGE_PATH" ]]; then
  echo "error: container did not create $APPIMAGE_PATH" >&2
  exit 1
fi
chmod 755 "$APPIMAGE_PATH"

"$CONTAINER_ENGINE" run --rm \
  --platform "$PLATFORM" \
  --env CARVERA_SIM_PACKAGE_SMOKE_DIR=/tmp/carvera-simulator-smoke \
  --volume "$ROOT_DIR:/work:ro" \
  --workdir /work \
  "$SMOKE_IMAGE_NAME" \
  ./scripts/smoke_test_linux_appimage.sh "/work/dist/linux/Carvera-Simulator-Linux-$ARTIFACT_ARCHITECTURE.AppImage"
