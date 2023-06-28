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

# To debug failed tests, please refer to
# https://github.com/googleapis/cloud-bigtable-clients-test

set -euo pipefail

source "$(dirname "$0")/../../../../ci/lib/init.sh"

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-args]"
  exit 1
fi

BAZEL_BIN="$1"
shift
bazel_args=("$@")

# Build and start the proxy in the background process. They are split as the
# test will wait for the proxy to be live, subject to timeout (e.g. 100s).
# Building the binary can easily take more than the timeout limit.
pushd "$(dirname "$0")/../test_proxy" >/dev/null
"${BAZEL_BIN}" build "${bazel_args[@]}" :cbt_test_proxy_main
"${BAZEL_BIN}" run "${bazel_args[@]}" :cbt_test_proxy_main -- 9999 >/dev/null 2>&1 &
proxy_pid=$!
popd >/dev/null

# Run the test
pushd /var/tmp/downloads/cloud-bigtable-clients-test/tests >/dev/null
go test -v -skip Generic_CloseClient -proxy_addr=:9999
exit_status=$?
# Remove the entire module cache, including unpacked source code of versioned
# dependencies.
go clean -modcache
popd >/dev/null

# Stop the proxy
kill $proxy_pid

exit "${exit_status}"
