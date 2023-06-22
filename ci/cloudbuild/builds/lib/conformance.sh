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

# This file defines helper functions for running conformance tests.
# Conformance test aims to validate different client libraries with the same
# set of tests. These client libraries are implemented in different languages,
# which requires client proxy to work with the tests.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_CONFORMANCE_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_CONFORMANCE_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_CONFORMANCE_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh

# Runs conformance tests with bazel using proxy.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   conformance::bazel_with_proxies "${args[@]}"
#
function conformance::bazel_with_proxies() {
  readonly PROXY_SCRIPT="run_conformance_tests_proxy_bazel.sh"

  local args=("${@:1}")

  io::log_h2 "Running Bigtable conformance tests (with proxy)"
  "google/cloud/bigtable/ci/${PROXY_SCRIPT}" bazel "${args[@]}"
}
