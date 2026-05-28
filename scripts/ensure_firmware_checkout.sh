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
DEFAULT_REPO_URL="$(tr -d '[:space:]' < "$ROOT_DIR/firmware/repository_url")"
DEFAULT_COMMIT="$(tr -d '[:space:]' < "$ROOT_DIR/firmware/compatible_commit")"

FIRMWARE_ROOT="${CARVERA_FIRMWARE_ROOT:-}"
FIRMWARE_DIR="${CARVERA_SIM_FIRMWARE_DIR:-"$ROOT_DIR/firmware/Carvera_Community_Firmware"}"
FIRMWARE_REPO="${CARVERA_SIM_FIRMWARE_REPO:-"$DEFAULT_REPO_URL"}"
FIRMWARE_COMMIT="${CARVERA_SIM_FIRMWARE_COMMIT:-"$DEFAULT_COMMIT"}"
ALLOW_UNPINNED="${CARVERA_ALLOW_UNPINNED_FIRMWARE:-0}"

usage() {
  cat <<EOF
Usage: scripts/ensure_firmware_checkout.sh [options]

Ensures the simulator-managed firmware checkout exists at the commit recorded in
firmware/compatible_commit, then prints the checkout path.

Options:
  --firmware-root PATH  Use an existing firmware checkout without cloning.
  --firmware-dir PATH   Managed checkout directory. Defaults to
                        ./firmware/Carvera_Community_Firmware.
  --allow-unpinned      Allow --firmware-root to point at a different commit.
  -h, --help            Show this help.

Environment alternatives:
  CARVERA_FIRMWARE_ROOT, CARVERA_SIM_FIRMWARE_DIR,
  CARVERA_SIM_FIRMWARE_REPO, CARVERA_SIM_FIRMWARE_COMMIT,
  CARVERA_ALLOW_UNPINNED_FIRMWARE
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
    --allow-unpinned)
      ALLOW_UNPINNED=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

validate_checkout() {
  local path="$1"
  if [[ ! -f "$path/src/main.cpp" ]]; then
    echo "error: firmware checkout is missing src/main.cpp: $path" >&2
    exit 2
  fi
}

allow_unpinned() {
  case "$ALLOW_UNPINNED" in
    1|[Oo][Nn]|[Tt][Rr][Uu][Ee]|[Yy][Ee][Ss])
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

validate_compatible_commit() {
  local path="$1"
  if ! command -v git >/dev/null || ! git -C "$path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    return
  fi

  local head
  head="$(git -C "$path" rev-parse HEAD)"
  if [[ "$head" == "$FIRMWARE_COMMIT" ]] || allow_unpinned; then
    return
  fi

  cat >&2 <<EOF
error: firmware checkout does not match compatible commit.
       $path is at $head,
       but this simulator is compatible with $FIRMWARE_COMMIT.
       Re-run without --firmware-root to use the managed checkout, or pass
       --allow-unpinned / CARVERA_ALLOW_UNPINNED_FIRMWARE=1 for firmware development.
EOF
  exit 2
}

if [[ -n "$FIRMWARE_ROOT" ]]; then
  FIRMWARE_ROOT="$(cd "$FIRMWARE_ROOT" && pwd)"
  validate_checkout "$FIRMWARE_ROOT"
  validate_compatible_commit "$FIRMWARE_ROOT"
  printf '%s\n' "$FIRMWARE_ROOT"
  exit 0
fi

command -v git >/dev/null || { echo "error: git is required to fetch the firmware checkout" >&2; exit 2; }

if [[ ! -d "$FIRMWARE_DIR/.git" ]]; then
  mkdir -p "$(dirname "$FIRMWARE_DIR")"
  git clone "$FIRMWARE_REPO" "$FIRMWARE_DIR" >&2
fi

if ! git -C "$FIRMWARE_DIR" cat-file -e "${FIRMWARE_COMMIT}^{commit}" 2>/dev/null; then
  git -C "$FIRMWARE_DIR" fetch origin "$FIRMWARE_COMMIT" >&2 || git -C "$FIRMWARE_DIR" fetch origin >&2
fi

git -C "$FIRMWARE_DIR" checkout --detach "$FIRMWARE_COMMIT" >&2
FIRMWARE_ROOT="$(cd "$FIRMWARE_DIR" && pwd)"
validate_checkout "$FIRMWARE_ROOT"
validate_compatible_commit "$FIRMWARE_ROOT"
printf '%s\n' "$FIRMWARE_ROOT"
