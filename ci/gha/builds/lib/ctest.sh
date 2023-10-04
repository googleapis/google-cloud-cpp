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

# Make our include guard clean against set -o nounset.
test -n "${CI_GHA_BUILDS_LIB_CTEST_SH__:-}" || declare -i CI_GHA_BUILDS_LIB_CTEST_SH__=0
if ((CI_GHA_BUILDS_LIB_CTEST_SH__++ != 0)); then
  return 0
fi # include guard

function ctest::common_args() {
  local args
  args=(
    # Print the full output on failures
    --output-on-failure
    # Run many tests in parallel, use -j for compatibility with old versions
    -j "$(os::cpus)"
    # Make the output shorter on interactive tests
    --progress
  )
  printf "%s\n" "${args[@]}"
}
