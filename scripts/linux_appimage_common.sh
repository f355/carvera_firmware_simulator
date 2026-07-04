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

linux_artifact_architecture() {
  case "$1" in
    aarch64 | arm64) printf 'arm64\n' ;;
    x86_64 | amd64) printf 'amd64\n' ;;
    *)
      printf 'error: unsupported Linux architecture: %s\n' "$1" >&2
      return 2
      ;;
  esac
}

linux_appimage_architecture() {
  case "$1" in
    aarch64 | arm64) printf 'aarch64\n' ;;
    x86_64 | amd64) printf 'x86_64\n' ;;
    *)
      printf 'error: unsupported Linux architecture: %s\n' "$1" >&2
      return 2
      ;;
  esac
}

linux_elf_machine_pattern() {
  case "$1" in
    aarch64 | arm64) printf 'AArch64\n' ;;
    x86_64 | amd64) printf 'Advanced Micro Devices X86-64\n' ;;
    *)
      printf 'error: unsupported Linux architecture: %s\n' "$1" >&2
      return 2
      ;;
  esac
}

linux_file_machine_pattern() {
  case "$1" in
    aarch64 | arm64) printf 'ARM aarch64\n' ;;
    x86_64 | amd64) printf 'x86-64\n' ;;
    *)
      printf 'error: unsupported Linux architecture: %s\n' "$1" >&2
      return 2
      ;;
  esac
}

linux_appimagetool_sha256() {
  case "$1" in
    aarch64) printf '1b00524ba8c6b678dc15ef88a5c25ec24def36cdfc7e3abb32ddcd068e8007fe\n' ;;
    x86_64) printf 'a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0\n' ;;
    *)
      printf 'error: unsupported appimagetool architecture: %s\n' "$1" >&2
      return 2
      ;;
  esac
}
