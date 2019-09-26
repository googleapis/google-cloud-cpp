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

if [ -z "${PROJECT_ROOT+x}" ]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/colors.sh"
source "${PROJECT_ROOT}/google/cloud/storage/tools/run_testbench_utils.sh"

# Create most likely unique names for the project and bucket so multiple tests
# can use the same testbench.
export PROJECT_ID="fake-project-${RANDOM}-${RANDOM}"
export BUCKET_NAME="fake-bucket-${RANDOM}-${RANDOM}"
export TOPIC_NAME="projects/${PROJECT_ID}/topics/fake-topic-${RANDOM}-${RANDOM}"
export LOCATION="fake-region1"
readonly HMAC_SERVICE_ACCOUNT="fake-service-account@example.com"
readonly SIGNING_SERVICE_ACCOUNT="fake-service-account@example.com"


readonly TEST_ACCOUNT_FILE="${PROJECT_ROOT}/google/cloud/storage/tests/UrlSignerV4TestAccount.json"
readonly TEST_DATA_FILE="${PROJECT_ROOT}/google/cloud/storage/tests/UrlSignerV4TestData.json"

echo
echo "Running Storage integration tests against local servers."
start_testbench

echo
echo "Running storage::internal::CurlRequest integration test."
./curl_request_integration_test

echo
echo "Running storage::internal::CurlRequestDownload integration test."
./curl_download_request_integration_test

echo
echo "Running storage::internal::CurlResumableUploadSession integration tests."
./curl_resumable_upload_session_integration_test "${BUCKET_NAME}"

echo
echo "Running CurlClient::SignBlob integration tests."
./curl_sign_blob_integration_test "${SIGNING_SERVICE_ACCOUNT}"

echo
echo "Running GCS Bucket APIs integration tests."
./bucket_integration_test "${PROJECT_ID}" "${BUCKET_NAME}" "${TOPIC_NAME}"

echo
echo "Running GCS Object Checksum integration tests."
./object_checksum_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object Hash integration tests."
./object_hash_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object Insert API integration tests."
./object_insert_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object APIs integration tests."
./object_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object file upload/download integration tests."
./object_file_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object file download multi-threaded test."
./object_file_multi_threaded_test "${BUCKET_NAME}" "128"

echo
echo "Running GCS Object media integration tests."
./object_media_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object resumable upload integration tests."
./object_resumable_write_integration_test "${BUCKET_NAME}"

echo
echo "Running GCS Object Rewrite integration tests."
./object_rewrite_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS multi-threaded integration test."
./thread_integration_test "${PROJECT_ID}" "${LOCATION}"

echo
echo "Running GCS Projects.serviceAccount integration tests."
./service_account_integration_test "${PROJECT_ID}" "${HMAC_SERVICE_ACCOUNT}"

echo
echo "Running V4 Signed URL conformance tests."
./signed_url_conformance_test "${TEST_ACCOUNT_FILE}" "${TEST_DATA_FILE}"

echo "Running Signed URL integration test."
./signed_url_integration_test "${BUCKET_NAME}" "${SIGNING_SERVICE_ACCOUNT}"

echo
echo "Running error injection integration tests."
./error_injection_integration_test "${BUCKET_NAME}"

# The tests were successful, so disable dumping of test bench log during
# shutdown.
TESTBENCH_DUMP_LOG=no

exit 0
