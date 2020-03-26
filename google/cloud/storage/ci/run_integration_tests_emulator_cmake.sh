#!/usr/bin/env bash
# Copyright 2020 Google LLC
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

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <binary-dir> [ctest-args]"
  exit 1
fi

BINARY_DIR="$(cd "${1}"; pwd)"
readonly BINARY_DIR
shift
ctest_args=("$@")

# Configure run_emulators_utils.sh to find the instance admin emulator.
CMDDIR="$(dirname "$0")"
readonly CMDDIR
PROJECT_ROOT="$(cd "${CMDDIR}/../../../.."; pwd)"
readonly PROJECT_ROOT
source "${PROJECT_ROOT}/google/cloud/storage/tools/run_testbench_utils.sh"

NONCE="$(date +%s)-${RANDOM}"
readonly NONCE
# Create most likely unique names for the project and bucket so multiple tests
# can use the same testbench.
export GOOGLE_CLOUD_PROJECT="fake-project-${NONCE}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="fake-bucket-${NONCE}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID="fake-region-${NONCE}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME="projects/${GOOGLE_CLOUD_PROJECT}/topics/fake-topic-${NONCE}"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT="fake-service-account@example.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="fake-service-account@example.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"

cd "${BINARY_DIR}"
start_testbench

ctest -L "storage-integration-tests" "${ctest_args[@]}"
exit_status=$?

kill_testbench
trap '' EXIT

exit "${exit_status}"
