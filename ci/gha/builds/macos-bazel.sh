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
source module ci/gha/builds/lib/macos.sh
# Define the os::* functions used in bazel.sh
source module ci/gha/builds/lib/bazel.sh
source module ci/lib/io.sh

# Usage: macos-bazel.sh [bazel query expression]
#
# The build compiles the targets found via `bazel query`. Recall that:
#    bazel query //a/...
# returns the targets matching the pattern `//a/...`.` Furthermore, the
# expressions can be combined using `+` and `-`, so:
#    bazel query //a/... +//b/... -//a/c/...
# Returns the targets that match `//a/...` or `//b/...`, but not `//a/c/...`.
#

mapfile -t args < <(bazel::common_args)
mapfile -t test_args < <(bazel::test_args)
mapfile -t integration_test_args < <(bazel::integration_test_args)

TIMEFORMAT="==> 🕑 bazel test done in %R seconds"

io::log_h1 "Get target list for: " "$@"
mapfile -t targets < <(bazelisk "${args[@]}" query -- "$@")

io::log_h1 "Starting Build"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  io::run bazelisk "${args[@]}" test "${test_args[@]}" \
    --test_tag_filters=-integration-test \
    //google/cloud:status_test "${targets[@]}"
}

TIMEFORMAT="==> 🕑 Storage integration tests done in %R seconds"
if [[ -n "${GHA_TEST_BUCKET:-}" ]]; then
  time {
    io::run bazelisk "${args[@]}" test "${test_args[@]}" "${integration_test_args[@]}" \
      //google/cloud/storage/tests/...
  }
fi
