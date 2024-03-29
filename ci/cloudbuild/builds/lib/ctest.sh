#!/bin/bash
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

# This bash library has various helper functions for our cmake-based builds.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CTEST_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CTEST_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CTEST_SH__++ != 0)); then
  return 0
fi # include guard

function ctest::common_args() {
  local args
  args=(
    # Print the full output on failures
    --output-on-failure
    # Run many tests in parallel, use -j for compatibility with old versions
    -j "$(nproc)"
    # Make the output shorter on interactive tests
    --progress
  )
  printf "%s\n" "${args[@]}"
}

function ctest::has_no_tests() {
  local dir="$1"
  local prefix="$2"
  shift 2
  local ctest_args=("$@")
  env -C "${dir}" ctest --show-only -R "${prefix}" "${ctest_args[@]}" 2>&1 | grep -q 'Total Tests: 0'
}
