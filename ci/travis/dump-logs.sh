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
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/travis/linux-config.sh"
source "${PROJECT_ROOT}/ci/define-dump-log.sh"

# Dump the image installation log.
echo
dump_log "cmake-out/install-linux.log"

# Dump the emulator log file. Tests run in the google/cloud/bigtable/tests directory.
echo
dump_log "${BUILD_OUTPUT}/google/cloud/bigtable/tests/emulator.log"
dump_log "${BUILD_OUTPUT}/google/cloud/bigtable/tests/instance-admin-emulator.log"
