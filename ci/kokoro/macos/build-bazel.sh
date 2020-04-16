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

if [[ $# -ne 1 ]]; then
  echo "Usage: $(basename "$0") <project-root>"
  exit 1
fi

readonly PROJECT_ROOT="$1"

source "${PROJECT_ROOT}/ci/colors.sh"
echo
echo "================================================================"
log_yellow "update or install Bazel."

# macOS does not have sha256sum by default, but `shasum -a 256` does the same
# thing:
function sha256sum() { shasum -a 256 "$@"; } && export -f sha256sum

mkdir -p "cmake-out/download"
(
  cd "cmake-out/download"
  "${PROJECT_ROOT}/ci/install-bazel.sh" >/dev/null
)

echo
echo "================================================================"
readonly BAZEL_BIN="$HOME/bin/bazel"
log_normal "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version
"${BAZEL_BIN}" shutdown

bazel_args=(
  # On macOS gRPC does not compile correctly unless one defines this:
  "--copt=-DGRPC_BAZEL_BUILD"
  # We need this environment variable because on macOS gRPC crashes if it
  # cannot find the credentials, even if you do not use them. Some of the
  # unit tests do exactly that.
  "--action_env=GOOGLE_APPLICATION_CREDENTIALS=${GOOGLE_APPLICATION_CREDENTIALS}"
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going")
if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=("--config" "${BAZEL_CONFIG}")
fi

echo
echo "================================================================"
for repeat in 1 2 3; do
  log_yellow "Fetch bazel dependencies [${repeat}/3]."
  if "${BAZEL_BIN}" fetch -- //google/cloud/...; then
    break
  else
    log_yellow "bazel fetch failed with $?"
  fi
done

echo
echo "================================================================"
log_yellow "build and run unit tests."
"${BAZEL_BIN}" test \
  "${bazel_args[@]}" "--test_tag_filters=-integration-tests" \
  -- //google/cloud/...:all

echo
echo "================================================================"
log_yellow "build all targets."
"${BAZEL_BIN}" build \
  "${bazel_args[@]}" -- //google/cloud/...:all
