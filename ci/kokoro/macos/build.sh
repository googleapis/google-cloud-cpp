#!/usr/bin/env bash
#
# Copyright 2018 Google LLC
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

echo "Reading CI secret configuration parameters."
source "${KOKORO_GFILE_DIR}/test-configuration.sh"

echo "Running build and tests"
cd "$(dirname $0)/../../.."
readonly PROJECT_ROOT="${PWD}"

echo
echo "================================================================"
echo "================================================================"
echo "Update Bazel."
echo

readonly BAZEL_VERSION=0.20.0
readonly GITHUB_DL="https://github.com/bazelbuild/bazel/releases/download"
wget -q "${GITHUB_DL}/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-darwin-x86_64.sh"
chmod +x "bazel-${BAZEL_VERSION}-installer-darwin-x86_64.sh"
./bazel-${BAZEL_VERSION}-installer-darwin-x86_64.sh --user

export PATH=$HOME/bin:$PATH
echo "which bazel: $(which bazel)"

# We need this environment variable because on macOS gRPC crashes if it cannot
# find the credentials, even if you do not use them. Some of the unit tests do
# exactly that.
echo
echo "================================================================"
echo "================================================================"
echo "Define GOOGLE_APPLICATION_CREDENTIALS."
export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"

# The -DGRPC_BAZEL_BUILD is needed because gRPC does not compile on macOS unless
# it is set.
bazel test \
    --copt=-DGRPC_BAZEL_BUILD \
    --action_env=GOOGLE_APPLICATION_CREDENTIALS="${GOOGLE_APPLICATION_CREDENTIALS}" \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //google/cloud/...:all

echo
echo "================================================================"
echo "================================================================"
bazel build \
    --copt=-DGRPC_BAZEL_BUILD \
    --action_env=GOOGLE_APPLICATION_CREDENTIALS="${GOOGLE_APPLICATION_CREDENTIALS}" \
    --test_output=errors \
    --verbose_failures=true \
    --keep_going \
    -- //google/cloud/...:all
