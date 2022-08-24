#!/usr/bin/env bash
#
# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

source "$(dirname "$0")/../lib/init.sh"
source module ci/lib/io.sh

if [[ $# -ne 1 ]]; then
  echo 2>"Usage: $(basename "$0") <library>"
  exit 1
fi

cd "${PROJECT_ROOT}"
readonly LIBRARY=$1

(
  sed '/<!-- inject-quickstart-start -->/q' "google/cloud/${LIBRARY}/README.md"
  echo ''
  echo '```cc'
  # Dumps the contents of quickstart.cc starting at the first #include, so we
  # skip the license header comment.
  sed -n -e '/END .*quickstart/,$d' -e '/^#/,$p' "google/cloud/${LIBRARY}/quickstart/quickstart.cc"
  echo '```'
  echo ''
  sed -n '/<!-- inject-quickstart-end -->/,$p' "google/cloud/${LIBRARY}/README.md"
) >"google/cloud/${LIBRARY}/README.md.tmp"
mv "google/cloud/${LIBRARY}/README.md.tmp" "google/cloud/${LIBRARY}/README.md"
