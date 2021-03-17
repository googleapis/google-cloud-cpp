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

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/etc/integration-tests-config.sh
source module /google/cloud/storage/tools/run_emulator_utils.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <binary-dir> [ctest-args]"
  exit 1
fi

BINARY_DIR="$(
  cd "${1}"
  pwd
)"
readonly BINARY_DIR
shift
ctest_args=("$@")

# Use the same configuration parameters as we use for testing against
# production. Easier to maintain just one copy.
export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes
export GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,100,WARNING"
export GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc"
export CLOUD_STORAGE_ENABLE_TRACING="raw-client"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT="fake-service-account-hmac@example.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT="fake-service-account-sign@example.com"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"

cd "${BINARY_DIR}"
start_emulator

# GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is automatically created, but we
# need to create the *DESTINATION_BUCKET_NAME too. Note that when the
# `storage_bucket_samples` binary is missing the examples that use said bucket
# are missing too.
if [[ -x "google/cloud/storage/examples/storage_bucket_samples" ]]; then
  google/cloud/storage/examples/storage_bucket_samples \
    create-bucket-for-project \
    "${GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME}" \
    "${GOOGLE_CLOUD_PROJECT}" >/dev/null
fi

ctest -R "^storage_" "${ctest_args[@]}"
exit_status=$?

kill_emulator
trap '' EXIT

if [[ "$exit_status" -ne 0 ]]; then
  source "${PROJECT_ROOT}/ci/define-dump-log.sh"
  dump_log "${HOME}/emulator.log"
fi

exit "${exit_status}"
