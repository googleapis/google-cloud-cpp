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
source module ci/gha/builds/lib/bazel.sh

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
# Do not run the integration tests
test_args+=(--test_tag_filters=-integration-test)
TIMEFORMAT="==> 🕑 bazel test done in %R seconds"

io::log_h1 "Get target list for: " "$@"
mapfile -t targets < <(bazelisk "${args[@]}" query -- "$@")

io::log_h1 "Starting Build"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  # See https://github.com/bazelbuild/bazel/issues/18965 we need to retry
  # because Bazel crashes sometimes.
  export -f io::run
  ci/retry-command.sh 3 1 \
    io::run bazelisk "${args[@]}" test "${test_args[@]}" -- //google/cloud:status_test "${targets[@]}"
}
