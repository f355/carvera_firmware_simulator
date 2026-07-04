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

if [[ $# -ne 8 ]]; then
  printf 'usage: %s SOURCE_ROOT BUILD_ROOT FIRMWARE_ROOT OUTPUT_ROOT CTEST JOBS UV GCOV_COMMAND\n' "$0" >&2
  exit 2
fi

source_root="$1"
build_root="$2"
firmware_root="$3"
output_root="$4"
ctest_command="$5"
test_jobs="$6"
uv_command="$7"
gcov_command="$8"

find "$build_root" -name '*.gcda' -delete
rm -rf "$output_root"
mkdir -p "$output_root/firmware-html" "$output_root/simulator-html"

test_status=0
"$ctest_command" --test-dir "$build_root" --output-on-failure -j "$test_jobs" || test_status=$?

gcovr=(
  "$uv_command" run --project "$source_root" gcovr
  "$build_root"
  --object-directory "$build_root"
  --gcov-executable "$gcov_command"
  --gcov-ignore-errors no_working_dir_found
  --gcov-ignore-parse-errors negative_hits.warn_once_per_file
  --gcov-ignore-parse-errors suspicious_hits.warn_once_per_file
  --merge-mode-functions merge-use-line-min
  --print-summary
  -j 0
)

"${gcovr[@]}" \
  --root "$firmware_root" \
  --filter "$firmware_root/src/" \
  --html-details "$output_root/firmware-html/index.html" \
  --html-title 'Carvera C1/CA1 firmware coverage' \
  --txt "$output_root/firmware.txt"

"${gcovr[@]}" \
  --root "$source_root" \
  --filter "$source_root/include/" \
  --filter "$source_root/src/" \
  --html-details "$output_root/simulator-html/index.html" \
  --html-title 'Carvera firmware simulator coverage' \
  --txt "$output_root/simulator.txt"

{
  printf '## C1/CA1 firmware coverage\n\n```text\n'
  sed -n '/^TOTAL/p' "$output_root/firmware.txt"
  printf '```\n\n## Simulator coverage\n\n```text\n'
  sed -n '/^TOTAL/p' "$output_root/simulator.txt"
  printf '```\n'
} > "$output_root/summary.md"

exit "$test_status"
