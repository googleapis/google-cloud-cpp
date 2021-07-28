#!/usr/bin/env bash
#
# Copyright 2021 Google LLC
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
source module /ci/lib/io.sh
source module /google/cloud/spanner/ci/lib/spanner_emulator.sh

if [[ $# -lt 2 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> <bazel-verb> [bazel-test-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift
bazel_test_args=("$@")

# Start the emulator and arranges to kill it, run in $HOME because
# spanner_emulator::start creates unsightly *.log files in the workspace
# otherwise. Use a fixed port so Bazel can cache the test results.
pushd "${HOME}" >/dev/null
spanner_emulator::start 8787
popd
trap spanner_emulator::kill EXIT

"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_env="SPANNER_EMULATOR_HOST=${SPANNER_EMULATOR_HOST}" \
  --test_tag_filters="integration-test" -- \
  "//google/cloud/spanner/...:all"
