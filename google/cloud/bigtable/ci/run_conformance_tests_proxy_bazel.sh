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

# Build and start the proxy
pushd . >/dev/null
cd "$(dirname "$0")/../test_proxy"
bazel build :cbt_test_proxy_main
nohup bazel run :cbt_test_proxy_main -- 9999 &
proxy_pid=$!
popd >/dev/null

# Run the test
pushd . >/dev/null
cd /var/tmp/downloads/cloud-bigtable-clients-test/tests
eval "go test -v -skip Generic_CloseClient -proxy_addr=:9999"
exit_status=$?
popd >/dev/null

# Stop the proxy
kill $proxy_pid
trap '' EXIT

exit "${exit_status}"
