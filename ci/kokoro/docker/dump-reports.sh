#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eu

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(
    cd "$(dirname "$0")/../../.."
    pwd
  )"
fi
source "${PROJECT_ROOT}/ci/kokoro/define-docker-variables.sh"

# If w3m is installed there is nothing to do.
if ! type w3m >/dev/null 2>&1; then
  # Try to install a HTML renderer, if this fails the script will exit. Note
  # that this runs on Kokoro, under Ubuntu, therefore the command to install
  # this package is well-known:
  sudo apt install -y w3m
fi

function dump_report() {
  local filename="$1"

  echo
  echo "================ ${filename} ================"
  w3m -dump "${filename}"
}

export -f dump_report

# Dumps the API/ABI compatibility report.
find "${BUILD_OUTPUT}" -name 'src_compat_report.html' \
  -exec bash -c 'dump_report "$1"' _ {} \; 2>/dev/null ||
  echo "No ABI compatibility reports found."
