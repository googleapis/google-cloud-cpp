#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/windows.sh
source module ci/gha/builds/lib/bazel.sh

# Usage: windows-bazel.sh <compilation-mode> [bazel query expression]
#
# The compilation mode is passed to Bazel via `--compilation_mode`.
#
# The build compiles the targets found via `bazel query`. Recall that:
#    bazel query //a/...
# returns the targets matching the pattern `//a/...`.` Furthermore, the
# expressions can be combined using `+` and `-`, so:
#    bazel query //a/... +//b/... -//a/c/...
# Returns the targets that match `//a/...` or `//b/...`, but not `//a/c/...`.
#

test_args+=("${msvc_args[@]}")
mapfile -t args < <(bazel::common_args)
mapfile -t test_args < <(bazel::test_args)
mapfile -t msvc_args < <(bazel::msvc_args)
test_args+=("${msvc_args[@]}")
# Do not run the integration tests
test_args+=(--test_tag_filters=-integration-test)
if [[ $# -gt 1 ]]; then
  test_args+=("--compilation_mode=${1}")
  shift
fi

if [[ -z "${VCINSTALLDIR}" ]]; then
  echo "ERROR: Missing VCINSTALLDIR, this is needed to configure Bazel+MSVC"
  exit 1
fi
export BAZEL_VC="${VCINSTALLDIR}"

io::log_h1 "Compute targets"
mapfile -t targets < <(bazelisk "${args[@]}" query -- "$@" | tr -d '\r' | sort)

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 bazel test done in %R seconds"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  io::log_bold bazelisk "${args[@]}" test "${test_args[@]}" -- //google/cloud:status_test "${targets[@]}"
  printf "%s\n" "${targets[@]}" | xargs -n 64 bazelisk "${args[@]}" test "${test_args[@]}" -- //google/cloud:status_test
}
