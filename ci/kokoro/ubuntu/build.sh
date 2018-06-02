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

export GOOGLE_APPLICATION_CREDENTIALS="${KOKORO_GFILE_DIR}/service-account.json"
source "${KOKORO_GFILE_DIR}/test-configuration.sh"

echo "Getting cbt tool"
export GOPATH="${KOKORO_ROOT}/golang"
go get -u cloud.google.com/go/bigtable/cmd/cbt

echo "Running build and tests"
cd "$(dirname $0)/../../.."
readonly PROJECT_ROOT="${PWD}"

git submodule update --init --recursive
cmake -DCMAKE_BUILD_TYPE=Release -H. -Bbuild-output
cmake --build build-output -- -j $(nproc)

cd build-output
ctest --output-on-failure

# The integration tests
export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="${PROJECT_ROOT}/third_party/grpc/etc/roots.pem"

echo "Running Google Cloud Bigtable Integration Tests"
(cd google/cloud/bigtable/tests && \
    "${PROJECT_ROOT}/google/cloud/bigtable/tests/run_integration_tests_production.sh")

echo "Running Google Cloud Storage Integration Tests"
(cd storage/tests && "${PROJECT_ROOT}/storage/tests/run_integration_tests.sh")
