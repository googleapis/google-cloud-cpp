#!/usr/bin/env bash
#
# Copyright 2022 Google LLC
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

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/lib/run_gcs_httpbin_emulator_utils.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift

# Separate caller-provided excluded targets (starting with "-//..."), so that
# we can make sure those appear on the command line after `--`.
bazel_test_args=()
for arg in "$@"; do
  bazel_test_args+=("${arg}")
done

# `start_emulator` creates unsightly *.log files in the current directory
# (which is ${PROJECT_ROOT}) and we cannot use a subshell because we want the
# environment variables that it sets.
pushd "${HOME}" >/dev/null
# Start the emulator on a fixed port, otherwise the Bazel cache gets
# invalidated on each run.
start_emulator 8888
popd >/dev/null

emulator_args=(
  "--test_env=HTTPBIN_ENDPOINT=${HTTPBIN_ENDPOINT}"
)

# We need to forward some environment variables suitable for running against
# the emulator.
"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" "${emulator_args[@]}" \
  "//google/cloud:internal_curl_rest_client_integration_test"

exit_status=$?

if [[ "$exit_status" -ne 0 ]]; then
  cat "${HOME}/gcs_emulator.log"
fi

exit "${exit_status}"
