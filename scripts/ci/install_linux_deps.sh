#!/usr/bin/env bash
#
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

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential \
  ccache \
  cmake \
  file \
  libasound2t64 \
  libgbm1 \
  libgl1-mesa-dri \
  libgtk-3-0t64 \
  libnss3 \
  libprotobuf-dev \
  librsvg2-bin \
  ninja-build \
  procps \
  protobuf-compiler \
  xvfb
