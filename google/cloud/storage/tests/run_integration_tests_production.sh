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

# The CI environment must provide the following environment variables:
# - GOOGLE_CLOUD_PROJECT: the name of a Google Cloud Platform project, the
#   integration tests should have Storage.Admin access to this project.
# - GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME: the name of a Google Cloud
#   Storage Bucket, the integration tests should have read/write access to this
#   bucket.
# - GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID: the name of of Google Cloud Storage
#   region, ideally close to the VMs running the integration tests.
# - GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT: a valid service account
#   in ${PROJECT_ID}. It is used to test HMAC keys. New keys will be created in
#   this account.
# - GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT: a valid service
#   account in ${PROJECT_ID}. It is used to sign URLs and policy documents. The
#   account must have enough privileges to sign blobs
#   (roles/iam.serviceAccountTokenCreator) and enough privileges to create and
#   access objects in ${GOOGLE_CLOUD_STORAGE_TEST_BUCKET_NAME}.
# - GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME: a valid Cloud Pub/Sub topic name,
#   configured to accept publications from the GCS service account in
#   ${GOOGLE_CLOUD_PROJECT}. It is used to create Cloud Pub/Sub notifications,
#   but no notifications are actually sent to it.

if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="$(cd "$(dirname "$0")/../../../.."; pwd)"
fi
readonly TEST_ACCOUNT_FILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
readonly TEST_DATA_FILE="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"

echo
echo "Running storage::internal::CurlResumableUploadSession integration tests."
./curl_resumable_upload_session_integration_test

echo
echo "Running CurlClient::SignBlob integration tests."
./curl_sign_blob_integration_test

echo
echo "Running GCS Bucket APIs integration tests."
./bucket_integration_test

echo
echo "Running GCS Object Checksum integration tests."
./object_checksum_integration_test

echo
echo "Running GCS Object Hash integration tests."
./object_hash_integration_test

echo
echo "Running GCS Object Insert API integration tests."
./object_insert_integration_test

echo
echo "Running GCS Object APIs integration tests."
./object_integration_test

echo
echo "Running GCS Object file upload/download integration tests."
./object_file_integration_test

echo
echo "Running GCS Object file download multi-threaded test."
./object_file_multi_threaded_test

echo
echo "Running GCS Object media integration tests."
./object_media_integration_test

echo
echo "Running GCS Object resumable upload integration tests."
./object_resumable_write_integration_test

echo
echo "Running GCS Object Rewrite integration tests."
./object_rewrite_integration_test

echo
echo "Running GCS Projects.serviceAccount integration tests."
./thread_integration_test

echo
echo "Running GCS Projects.serviceAccount integration tests."
./service_account_integration_test

echo
echo "Running V4 Signed URL conformance tests."
./signed_url_conformance_test

echo "Running Signed URL integration test."
./signed_url_integration_test

echo "Running JSON keyfile integration test."
env "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILENAME=${TEST_KEY_FILE_JSON}" \
    ./key_file_integration_test

echo "Running P12 keyfile integration test."
env "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILENAME=${TEST_KEY_FILE_P12}" \
    ./key_file_integration_test

echo
echo "Running GCS Object APIs with P12 credentials."
env "GOOGLE_APPLICATION_CREDENTIALS=${TEST_KEY_FILE_P12}" \
    ./object_integration_test
