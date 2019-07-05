#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# This script is supposed to run inside a Docker container, see
# ci/kokoro/build.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="/v"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
echo "${COLOR_YELLOW}Starting docker build $(date) with ${NCPU} cores${COLOR_RESET}"
echo

echo "================================================================"
readonly BAZEL_BIN="/usr/local/bin/bazel"
echo "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version
echo "================================================================"

bazel_args=("--test_output=errors" "--verbose_failures=true" "--keep_going")
if [[ -n "${BAZEL_CONFIG}" ]]; then
    bazel_args+=("--config" "${BAZEL_CONFIG}")
fi

echo "================================================================"
echo "Compiling and running unit tests $(date)"
echo "================================================================"
"${BAZEL_BIN}" test \
    "${bazel_args[@]}" \
    -- //google/cloud/...:all

echo "================================================================"
echo "Compiling all the code, including integration tests $(date)"
echo "================================================================"
# Then build everything else (integration tests, examples, etc). So we can run
# them next.
"${BAZEL_BIN}" build \
    "${bazel_args[@]}" \
    -- //google/cloud/...:all

echo "================================================================"
echo "Build finished at $(date)"
echo "================================================================"

exit 0
