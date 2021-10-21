#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/etc/integration-tests-config.sh
source module /ci/lib/io.sh
source module /google/cloud/spanner/ci/lib/spanner_emulator.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <binary-dir> [ctest-args]"
  exit 1
fi

# The first argument is where the build scripts have put the binaries, i.e., the
# -B argument to the cmake configuration step.
CMAKE_BINARY_DIR="$(realpath "${1}")"
readonly CMAKE_BINARY_DIR
shift

# Any additional arguments for the ctest invocation.
ctest_args=("$@")

cd "${CMAKE_BINARY_DIR}"

# Starts the emulator and arranges to kill it.
spanner_emulator::start
trap spanner_emulator::kill EXIT

ctest -R "^spanner_" "${ctest_args[@]}"
exit_status=$?

exit "${exit_status}"
