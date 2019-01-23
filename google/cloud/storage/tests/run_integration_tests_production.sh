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
# - PROJECT_ID: the name of a Google Cloud Platform project, the integration
#   tests should have Storage.Admin access to this project.
# - BUCKET_NAME: the name of a Google Cloud Storage Bucket, the integration
#   tests should have read/write access to this bucket.
# - STORAGE_REGION_ID: the name of of Google Cloud Storage region, ideally close
#   to the VMs running the integration tests.

echo
echo "Running storage::internal::CurlResumableUploadSession integration tests."
./curl_resumable_upload_session_integration_test "${BUCKET_NAME}"

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
echo "Running GCS Object media integration tests."
./object_media_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Object resumable upload integration tests."
./object_resumable_write_integration_test "${BUCKET_NAME}"

echo
echo "Running GCS Object Rewrite integration tests."
./object_rewrite_integration_test "${PROJECT_ID}" "${BUCKET_NAME}"

echo
echo "Running GCS Projects.serviceAccount integration tests."
./thread_integration_test "${PROJECT_ID}" "${STORAGE_REGION_ID}"

echo
echo "Running GCS Projects.serviceAccount integration tests."
./service_account_integration_test "${PROJECT_ID}"
