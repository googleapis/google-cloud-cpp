#!/usr/bin/env bash
#
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

echo "Building google-cloud-cpp as a dependency of another project $(date)."
cd "$(dirname "$0")/../../../.."
readonly PROJECT_ROOT="${PWD}"

echo "================================================================"
echo "Update or Install Bazel $(date)."
echo "================================================================"
# We ping the version of Bazel because we do not want all our builds to break
# when Kokoro updates Bazel. We rather upgrade our tooling when *we* decide it
# is a good time to do so.
"${PROJECT_ROOT}/ci/install-bazel.sh"

readonly BAZEL_BIN="$HOME/bin/bazel"
echo "Using Bazel in ${BAZEL_BIN}"

echo "================================================================"
echo "Compile the project in ci/test-install $(date)."
echo "================================================================"
(cd ci/test-install ; "${BAZEL_BIN}" build \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //...:all)
