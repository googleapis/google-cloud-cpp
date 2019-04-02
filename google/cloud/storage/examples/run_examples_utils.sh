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
  readonly PROJECT_ROOT="$(cd "$(dirname $0)/../../../.."; pwd)"
fi
source "${PROJECT_ROOT}/ci/define-example-runner.sh"

################################################
# Run all Bucket examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_bucket_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  local object_name="object-${RANDOM}-${RANDOM}.txt"

  run_example ./storage_bucket_samples list-buckets-for-project \
      "${PROJECT_ID}"
  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples get-bucket-metadata \
      "${bucket_name}"
  run_example ./storage_bucket_samples change-default-storage-class \
      "${bucket_name}" "NEARLINE"
  run_example ./storage_bucket_samples patch-bucket-storage-class \
      "${bucket_name}" "STANDARD"
  run_example ./storage_bucket_samples patch-bucket-storage-class-with-builder \
      "${bucket_name}" "COLDLINE"
  run_example ./storage_bucket_samples enable-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples disable-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples add-bucket-label \
      "${bucket_name}" "test-label" "test-label-value"
  run_example ./storage_bucket_samples get-bucket-labels \
      "${bucket_name}"
  run_example ./storage_bucket_samples remove-bucket-label \
      "${bucket_name}" "test-label"
  run_example ./storage_bucket_samples get-bucket-labels \
      "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"

  # Run the examples where the project id is obtained from the environment:
  export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
  run_example ./storage_bucket_samples list-buckets
  run_example ./storage_bucket_samples create-bucket "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-metadata "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-metadata "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"

  run_example ./storage_bucket_samples create-bucket-with-storage-class-location \
      "${bucket_name}" "STANDARD" "US"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
  unset GOOGLE_CLOUD_PROJECT

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_bucket_samples
}

################################################
# Run all examples about service accounts.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_service_account_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_service_account_samples \
      get-service-account-for-project "${PROJECT_ID}"

  # Run the examples where the project id is obtained from the environment:
  export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
  run_example ./storage_service_account_samples get-service-account
  unset GOOGLE_CLOUD_PROJECT

  if [[ -z "${CLOUD_STORAGE_TESTBENCH_ENDPOINT:-}" ]]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " HMAC key examples skipped because they are not enabled in production."
    return
  fi

  run_example ./storage_service_account_samples \
      create-hmac-key-for-project "${PROJECT_ID}" "${SERVICE_ACCOUNT}"

  # Run the examples using the environment to get the project id:
  export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
  run_example ./storage_service_account_samples \
      create-hmac-key "${SERVICE_ACCOUNT}"
  run_example ./storage_service_account_samples \
      list-hmac-keys
  run_example ./storage_service_account_samples \
      list-hmac-keys-with-service-account "${SERVICE_ACCOUNT}"
  # Use any of the HMAC keys to test `get-hmac-key`.
  local access_id
  access_id=$(
      ./storage_service_account_samples \
          list-hmac-keys-with-service-account "${SERVICE_ACCOUNT}" | \
          sed -n 's;^access_id = \(.*\);\1;p' | head -1)
  run_example ./storage_service_account_samples \
      get-hmac-key "${access_id}"
  run_example ./storage_service_account_samples \
      update-hmac-key "${access_id}" "INACTIVE"
  run_example ./storage_service_account_samples \
      update-hmac-key "${access_id}" "ACTIVE"
  # Parse the output to delete all the keys created in this service account.
  for access_id in $(
      ./storage_service_account_samples \
          list-hmac-keys-with-service-account "${SERVICE_ACCOUNT}" | \
          sed -n 's;^access_id = \(.*\);\1;p'); do
    run_example ./storage_service_account_samples \
        delete-hmac-key "${access_id}"
  done

  unset GOOGLE_CLOUD_PROJECT
}

################################################
# Run all examples using a Requester Pays bucket.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_requester_pays_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  local object_name="object-${RANDOM}-${RANDOM}.txt"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_bucket_samples get-billing \
      "${bucket_name}"
  run_example ./storage_bucket_samples enable-requester-pays \
      "${bucket_name}"
  run_example ./storage_bucket_samples write-object-requester-pays \
      "${bucket_name}" "${object_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples read-object-requester-pays \
      "${bucket_name}" "${object_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples delete-object-requester-pays \
      "${bucket_name}" "${object_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples disable-requester-pays \
      "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
}

################################################
# Run all examples showing how to use default event based holds.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_default_event_based_hold_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples get-default-event-based-hold \
      "${bucket_name}"
  run_example ./storage_bucket_samples enable-default-event-based-hold \
      "${bucket_name}"
  run_example ./storage_bucket_samples disable-default-event-based-hold \
      "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
}

################################################
# Run all examples showing how to use retention policies.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_retention_policy_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples get-retention-policy \
      "${bucket_name}"
  run_example ./storage_bucket_samples set-retention-policy \
      "${bucket_name}" 30
  run_example ./storage_bucket_samples remove-retention-policy \
      "${bucket_name}"
  run_example ./storage_bucket_samples set-retention-policy \
      "${bucket_name}" 30
  run_example ./storage_bucket_samples lock-retention-policy \
      "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
}

################################################
# Run all Bucket ACL examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
###############################################
run_all_bucket_acl_examples() {
  # Use a fresh bucket to avoid flaky tests due to other tests also making
  # changes on the bucket.
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_bucket_acl_samples list-bucket-acl \
      "${bucket_name}"
  run_example ./storage_bucket_acl_samples create-bucket-acl \
      "${bucket_name}" allAuthenticatedUsers READER
  run_example ./storage_bucket_acl_samples get-bucket-acl \
      "${bucket_name}" allAuthenticatedUsers
  run_example ./storage_bucket_acl_samples update-bucket-acl \
      "${bucket_name}" allAuthenticatedUsers OWNER
  run_example ./storage_bucket_acl_samples patch-bucket-acl \
      "${bucket_name}" allAuthenticatedUsers READER
  run_example ./storage_bucket_acl_samples patch-bucket-acl-no-read \
      "${bucket_name}" allAuthenticatedUsers OWNER
  run_example ./storage_bucket_acl_samples delete-bucket-acl \
      "${bucket_name}" allAuthenticatedUsers

  run_example ./storage_bucket_acl_samples add-bucket-owner \
      "${bucket_name}" allAuthenticatedUsers
  run_example ./storage_bucket_acl_samples remove-bucket-owner \
      "${bucket_name}" allAuthenticatedUsers

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_bucket_acl_samples
}

################################################
# Run all Default Object ACL examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_default_object_acl_examples() {
  # Use a fresh bucket to avoid flaky tests due to other tests also making
  # changes on the bucket.
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_default_object_acl_samples list-default-object-acl \
      "${bucket_name}"
  run_example ./storage_default_object_acl_samples create-default-object-acl \
      "${bucket_name}" allAuthenticatedUsers READER
  run_example ./storage_default_object_acl_samples get-default-object-acl \
      "${bucket_name}" allAuthenticatedUsers
  run_example ./storage_default_object_acl_samples update-default-object-acl \
      "${bucket_name}" allAuthenticatedUsers OWNER
  run_example ./storage_default_object_acl_samples patch-default-object-acl \
      "${bucket_name}" allAuthenticatedUsers READER
  run_example ./storage_default_object_acl_samples patch-default-object-acl-no-read \
      "${bucket_name}" allAuthenticatedUsers OWNER
  run_example ./storage_default_object_acl_samples delete-default-object-acl \
      "${bucket_name}" allAuthenticatedUsers

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_default_object_acl_samples
}

################################################
# Run all Object examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_object_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"
  local bucket_prefix="prefix-${RANDOM}-${RANDOM}"
  local composed_object_name="composed-object-${RANDOM}-${RANDOM}.txt"
  local copied_object_name="copied-object-${RANDOM}-${RANDOM}.txt"
  local multipart_object_name="multipart-object-${RANDOM}-${RANDOM}.txt"

  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples list-objects "${bucket_name}"
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${bucket_prefix}/object-1.txt" "media-for-object-1"
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${bucket_prefix}/object-2.txt" "media-for-object-2"
  run_example ./storage_object_samples list-objects-with-prefix \
      "${bucket_name}" "${bucket_prefix}"
  run_example ./storage_object_samples get-object-metadata \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples change-object-storage-class \
      "${bucket_name}" "${object_name}" "NEARLINE"
  run_example ./storage_object_samples read-object \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples write-object \
      "${bucket_name}" "${object_name}" 100000
   run_example ./storage_object_samples read-object-range \
      "${bucket_name}" "${object_name}" "1000" "2000"
  run_example ./storage_object_samples update-object-metadata \
      "${bucket_name}" "${object_name}" "test-label" "test-value"
  run_example ./storage_object_samples patch-object-content-type \
      "${bucket_name}" "${object_name}" "application/text"
  run_example ./storage_object_samples patch-object-delete-metadata \
      "${bucket_name}" "${object_name}" "test-label"
  run_example ./storage_object_samples compose-object \
      "${bucket_name}" "${composed_object_name}" "${object_name}" \
      "${object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${composed_object_name}"
  run_example ./storage_object_samples copy-object \
      "${bucket_name}" "${object_name}" \
      "${bucket_name}" "${copied_object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${copied_object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${bucket_prefix}/object-2.txt"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${bucket_prefix}/object-1.txt"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"

  run_example ./storage_object_samples insert-object-multipart \
      "${bucket_name}" "${multipart_object_name}" \
      "text/plain" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${multipart_object_name}"

  local encrypted_object_name="enc-obj-${RANDOM}-${RANDOM}.txt"
  local encrypted_composed_object_name="composed-enc-obj-${RANDOM}-${RANDOM}.txt"
  local encrypted_copied_object_name="copied-enc-obj-${RANDOM}-${RANDOM}.txt"

  local key="$(./storage_object_samples generate-encryption-key |
      grep 'Base64 encoded key' | awk '{print $5}')"
  run_example ./storage_object_samples write-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" "${key}"
  run_example ./storage_object_samples read-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" "${key}"
  run_example ./storage_object_samples compose-object-from-encrypted-objects \
      "${bucket_name}" "${encrypted_composed_object_name}" "${key}" \
      "${encrypted_object_name}" "${encrypted_object_name}"
  run_example ./storage_object_samples read-encrypted-object \
      "${bucket_name}" "${encrypted_composed_object_name}" "${key}"
  run_example ./storage_object_samples copy-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" \
      "${bucket_name}" "${encrypted_copied_object_name}" "${key}"
  run_example ./storage_object_samples read-encrypted-object \
      "${bucket_name}" "${encrypted_copied_object_name}" "${key}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${encrypted_copied_object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${encrypted_composed_object_name}"

  local newkey="$(./storage_object_samples generate-encryption-key |
      grep 'Base64 encoded key' | awk '{print $5}')"
  run_example ./storage_object_samples rotate-encryption-key \
      "${bucket_name}" "${encrypted_object_name}" "${key}" "${newkey}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${encrypted_object_name}"

  local object_name_strict="object-strict-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object-strict-idempotency \
      "${bucket_name}" "${object_name_strict}" \
      "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name_strict}"

  local object_name_retry="object-retry-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object-modified-retry \
      "${bucket_name}" "${object_name_retry}" \
      "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name_retry}"
}

################################################
# Run upload and download examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_upload_and_download_examples() {
  local bucket_name=$1
  shift

  local object_name="uploaded-${RANDOM}-${RANDOM}.txt"
  local upload_file_name="$(mktemp -t "upload.XXXXXX")"
  local download_file_name="$(mktemp -t "download.XXXXXX")"
  cat > "${upload_file_name}" <<_EOF_
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
culpa qui officia deserunt mollit anim id est laborum.
_EOF_

  run_example ./storage_object_samples upload-file \
      "${upload_file_name}" "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples download-file \
      "${bucket_name}" "${object_name}" "${download_file_name}"
  diff "${upload_file_name}" "${download_file_name}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run resumable file upload examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_resumable_file_upload_examples() {
  local bucket_name=$1
  shift

  local object_name="uploaded-resumable-${RANDOM}-${RANDOM}.txt"
  local upload_file_name="$(mktemp -t "upload.XXXXXX")"
  local download_file_name="$(mktemp -t "download.XXXXXX")"
  cat > "${upload_file_name}" <<_EOF_
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis
nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
culpa qui officia deserunt mollit anim id est laborum.
_EOF_

  run_example ./storage_object_samples upload-file-resumable \
      "${upload_file_name}" "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples download-file \
      "${bucket_name}" "${object_name}" "${download_file_name}"
  diff "${upload_file_name}" "${download_file_name}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run resumable write object examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_resumable_write_object_examples() {
  local bucket_name=$1
  shift

  local object_name="resumable-upload-${RANDOM}-${RANDOM}.txt"

  # We need to capture the output, so the usual `run_example` helper does not
  # help here :-)
  set +e
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        " storage_object_samples start-resumable-upload"
  local session_id
  session_id=$(./storage_object_samples start-resumable-upload \
      "${bucket_name}" "${object_name}" | \
      sed "s/Created resumable upload: //")
  if [[ $? = 0 ]]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " storage_object_samples start-resumable-upload"
  else
    echo   "${COLOR_RED}[   FAILED ]${COLOR_RESET}" \
        " storage_object_samples start-resumable-upload"
  fi
  run_example ./storage_object_samples resume-resumable-upload \
      "${bucket_name}" "${object_name}" "${session_id}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the example showing how to rewrite one object.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   source_bucket_name: an existing bucket where the source object will be
#     created.
#   target_bucket_name: an existing bucket where the target object will be
#     created.
# Returns:
#   None
################################################
run_rewrite_object_example() {
  local source_bucket_name=$1
  local target_bucket_name=$2
  shift 2

  local source_object_name="rewrite-source-object-${RANDOM}-${RANDOM}.txt"
  local target_object_name="rewrite-target-object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object \
      "${source_bucket_name}" "${source_object_name}" \
      "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples rewrite-object \
      "${source_bucket_name}" "${source_object_name}" \
      "${source_bucket_name}" "${target_object_name}"
  run_example ./storage_object_samples delete-object \
      "${source_bucket_name}" "${target_object_name}"
  run_example ./storage_object_samples delete-object \
      "${source_bucket_name}" "${source_object_name}"
}

################################################
# Run the example showing how to rename one object.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   source_bucket_name: an existing bucket where the source object will be
#     created and then renamed.
# Returns:
#   None
################################################
run_rename_object_example() {
  local source_bucket_name=$1
  shift

  local source_object_name="rename-source-object-${RANDOM}-${RANDOM}.txt"
  local target_object_name="rename-target-object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object \
      "${source_bucket_name}" "${source_object_name}" \
      "a-string-to-serve-as-object-media-in-rename-example"
  run_example ./storage_object_samples rename-object \
      "${source_bucket_name}" "${source_object_name}" "${target_object_name}"
  run_example ./storage_object_samples delete-object \
      "${source_bucket_name}" "${target_object_name}"
}

################################################
# Run the example showing how to resume a partially completed rewrite.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   source_bucket_name: an existing bucket where the source object will be
#     created.
#   target_bucket_name: an existing bucket where the target object will be
#     created.
# Returns:
#   None
################################################
run_resume_rewrite_example() {
  local source_bucket_name=$1
  local target_bucket_name=$2
  shift 2

  local source_object_name="rewrite-resume-source-object-${RANDOM}-${RANDOM}.txt"
  local target_object_name="rewrite-resume-target-object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples write-large-object \
      "${source_bucket_name}" "${source_object_name}" "16"
  local msg=$(./storage_object_samples rewrite-object-token \
      "${source_bucket_name}" "${source_object_name}" \
      "${target_bucket_name}" "${target_object_name}")

  if echo "${msg}" | grep -q "Rewrite in progress"; then
    local token=$(echo ${msg} | awk '{print $5}')
    run_example ./storage_object_samples rewrite-object-resume \
        "${source_bucket_name}" "${source_object_name}" \
        "${target_bucket_name}" "${target_object_name}" "${token}"
  else
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " rewrite-object-resume the rewrite completed in one step."
  fi
  run_example ./storage_object_samples delete-object \
      "${target_bucket_name}" "${target_object_name}"
  run_example ./storage_object_samples delete-object \
      "${source_bucket_name}" "${source_object_name}"
}

################################################
# Run the examples showing how to rewrite objects.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   source_bucket_name: an existing bucket where the source object will be
#     created.
#   target_bucket_name: an existing bucket where the target object will be
#     created.
# Returns:
#   None
################################################
run_all_object_rewrite_examples() {
  local source_bucket_name=$1
  local target_bucket_name=$2
  shift 2

  run_rewrite_object_example "${source_bucket_name}" "${target_bucket_name}"
  run_rename_object_example "${source_bucket_name}"
  run_resume_rewrite_example "${source_bucket_name}" "${target_bucket_name}"
}

################################################
# Run the example showing how to make objects public.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_public_object_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples make-object-public \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples read-object-unauthenticated \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the examples showing how to use event based holds.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_event_based_hold_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples set-event-based-hold \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples release-event-based-hold \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the examples showing how to use temporary holds.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_temporary_hold_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples set-temporary-hold \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples release-temporary-hold \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the examples showing how to use object versioning.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_object_versioning_examples() {
  # Create the bucket to avoid changing the configuration for "${BUCKET_NAME}"
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  local object_name="object-${RANDOM}-${RANDOM}.txt"
  local copied_object_name="object-${RANDOM}-${RANDOM}.txt"
  run_example ./storage_bucket_samples enable-object-versioning \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-object-versioning \
      "${bucket_name}"
  local archived_generation
  archived_generation=$(./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media" | \
      sed -n 's/.*, generation=\([0-9]*\).*/\1/p')
  local live_generation
  live_generation=$(./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media" | \
      sed -n 's/.*, generation=\([0-9]*\).*/\1/p')
  run_example ./storage_object_samples copy-versioned-object \
      "${bucket_name}" "${object_name}" \
      "${bucket_name}" "${copied_object_name}" "${archived_generation}"
  run_example ./storage_object_samples delete-versioned-object \
      "${bucket_name}" "${object_name}" "${archived_generation}"
  run_example ./storage_object_samples delete-versioned-object \
      "${bucket_name}" "${object_name}" "${live_generation}"
  run_example ./storage_bucket_samples disable-object-versioning \
      "${bucket_name}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${copied_object_name}"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"
}

################################################
# Run all Customer-managed Encryption Keys examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   cmek: the name of the Customer-managed Encryption Key used in the tests.
# Returns:
#   None
################################################
run_all_cmek_examples() {
  local cmek=$1

  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  local encrypted_object_name="enc-obj-${RANDOM}-${RANDOM}.txt"
  local object_name="object-${RANDOM}-${RANDOM}.txt"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_object_samples write-object-with-kms-key \
      "${bucket_name}" "${object_name}" "${cmek}"
  run_example ./storage_object_samples get-object-kms-key \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples read-object \
      "${bucket_name}" "${object_name}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"

  local key="$(./storage_object_samples generate-encryption-key |
      grep 'Base64 encoded key' | awk '{print $5}')"
  run_example ./storage_object_samples write-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" "${key}"
  run_example ./storage_object_samples object-csek-to-cmek \
      "${bucket_name}" "${encrypted_object_name}" "${key}" "${cmek}"
  run_example ./storage_object_samples read-object \
      "${bucket_name}" "${object_name}"

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${encrypted_object_name}"

  run_example ./storage_bucket_samples get-bucket-default-kms-key \
      "${bucket_name}"
  run_example ./storage_bucket_samples add-bucket-default-kms-key \
      "${bucket_name}" "${cmek}"
  run_example ./storage_bucket_samples get-bucket-default-kms-key \
      "${bucket_name}"
  run_example ./storage_bucket_samples remove-bucket-default-kms-key \
      "${bucket_name}"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"
}

################################################
# Run the examples to create V2 signed urls.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_signed_url_v2_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"

  if [[ -n "${CLOUD_STORAGE_TESTBENCH_ENDPOINT:-}" ]]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " V2 Signed URL examples disabled when using the testbench."
    return
  fi

  run_example ./storage_object_samples create-put-signed-url-v2 \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples create-get-signed-url-v2 \
      "${bucket_name}" "${object_name}"

  local magic_string="${RANDOM}-some-data-to-serve-as-object-media"

  set +e
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        " using PUT signed URL"
  local put_url
  put_url=$(./storage_object_samples create-put-signed-url-v2 \
      "${bucket_name}" "${object_name}" | head -1 | \
      sed "s/The signed url is: //")
  curl --silent -X PUT -H 'Content-Type: application/octet-stream' \
      "${put_url}" -d "${magic_string}"
  if [[ $? = 0 ]]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " using PUT signed URL"
  else
    echo   "${COLOR_RED}[   FAILED ]${COLOR_RESET}" \
        " using PUT signed URL"
  fi
  set -e

  set +e
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        " using GET signed URL"
  local get_url
  get_url=$(./storage_object_samples create-get-signed-url-v2 \
      "${bucket_name}" "${object_name}" | head -1 | \
      sed "s/The signed url is: //")
  if curl --silent "${get_url}" | grep -q "${magic_string}"; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " using PUT signed URL"
  else
    echo   "${COLOR_RED}[   FAILED ]${COLOR_RESET}" \
        " using PUT signed URL"
  fi
  set -e

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the examples to create V4 signed urls.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_signed_url_v4_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"

  if [[ -n "${CLOUD_STORAGE_TESTBENCH_ENDPOINT:-}" ]]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " V4 Signed URL examples disabled when using the testbench."
    return
  fi

  run_example ./storage_object_samples create-put-signed-url-v4 \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples create-get-signed-url-v4 \
      "${bucket_name}" "${object_name}"

  local magic_string="${RANDOM}-some-data-to-serve-as-object-media"

  set +e
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        " using PUT signed URL"
  local put_url
  put_url=$(./storage_object_samples create-put-signed-url-v4 \
      "${bucket_name}" "${object_name}" | head -1 | \
      sed "s/The signed url is: //")
  curl --silent -X PUT -H 'Content-Type: application/octet-stream' \
      "${put_url}" -d "${magic_string}"
  if [[ $? = 0 ]]; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " using PUT signed URL"
  else
    echo   "${COLOR_RED}[   FAILED ]${COLOR_RESET}" \
        " using PUT signed URL"
  fi
  set -e

  set +e
  echo    "${COLOR_GREEN}[ RUN      ]${COLOR_RESET}" \
        " using GET signed URL"
  local get_url
  get_url=$(./storage_object_samples create-get-signed-url-v4 \
      "${bucket_name}" "${object_name}" | head -1 | \
      sed "s/The signed url is: //")
  if curl --silent "${get_url}" | grep -q "${magic_string}"; then
    echo "${COLOR_GREEN}[       OK ]${COLOR_RESET}" \
        " using PUT signed URL"
  else
    echo   "${COLOR_RED}[   FAILED ]${COLOR_RESET}" \
        " using PUT signed URL"
  fi
  set -e

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"
}

################################################
# Run the examples to create signed policy documents.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None 
# Returns:
#   None
################################################
run_signed_policy_document_examples() {
  if [[ -n "${CLOUD_STORAGE_TESTBENCH_ENDPOINT:-}" ]]; then
    echo "${COLOR_YELLOW}[  SKIPPED ]${COLOR_RESET}" \
        " Signed policy document examples disabled when using the testbench."
    return
  fi

  run_example ./storage_bucket_samples create-signed-policy-document
}

################################################
# Run all Object ACL examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_object_acl_examples() {
  local bucket_name=$1
  shift

  local object_name="object-${RANDOM}-${RANDOM}.txt"

  # We need to create an object to run the examples on.
  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "some-string-to-serve-as-object-media"

  run_example ./storage_object_acl_samples list-object-acl \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_acl_samples create-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers READER
  run_example ./storage_object_acl_samples get-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers
  run_example ./storage_object_acl_samples update-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers OWNER
  run_example ./storage_object_acl_samples patch-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers READER
  run_example ./storage_object_acl_samples patch-object-acl-no-read \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers OWNER
  run_example ./storage_object_acl_samples delete-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers

  run_example ./storage_object_acl_samples add-object-owner \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers
  run_example ./storage_object_acl_samples get-object-acl \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers
  run_example ./storage_object_acl_samples remove-object-owner \
      "${bucket_name}" "${object_name}" allAuthenticatedUsers

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_object_acl_samples
}

################################################
# Run all Notification examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   topic_name: the topic used to create notifications.
# Returns:
#   None
################################################
run_all_notification_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  local topic_name=$1
  shift

  # Create a new bucket for each run so the list of notifications is initially
  # empty
  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_notification_samples create-notification \
      "${bucket_name}" "${topic_name}"
  run_example ./storage_notification_samples list-notifications \
      "${bucket_name}"
  # The notifications ids are assigned by the server, so we need to discover it
  # here. Parse the output from the list-notifications command to extract what
  # we need.
  local id="$(./storage_notification_samples list-notifications \
      "${bucket_name}" | egrep -o 'id=[^,]*' | sed 's/id=//')"
  run_example ./storage_notification_samples get-notification \
      "${bucket_name}" "${id}"
  run_example ./storage_notification_samples delete-notification \
      "${bucket_name}" "${id}"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_notification_samples
}

################################################
# Run all Bucket IAM examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_bucket_iam_examples() {
  # Use a fresh bucket to avoid flaky tests due to other tests also making
  # changes on the bucket.
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"
  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_bucket_iam_samples get-bucket-iam-policy \
      "${bucket_name}"
  run_example ./storage_bucket_iam_samples add-bucket-iam-member \
      "${bucket_name}" "roles/storage.objectViewer" "allAuthenticatedUsers"
  run_example ./storage_bucket_iam_samples remove-bucket-iam-member \
      "${bucket_name}" "roles/storage.objectViewer" "allAuthenticatedUsers"
  run_example ./storage_bucket_iam_samples test-bucket-iam-permissions \
      "${bucket_name}" "storage.objects.list" "storage.objects.delete"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_bucket_iam_samples
}

################################################
# Run the examples showing how to use static website configuration.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_static_website_configuration_examples() {
  # Create the bucket to avoid changing the configuration for "${BUCKET_NAME}"
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_bucket_samples set-static-website-configuration \
      "${bucket_name}" "main-page.html" "not-found.html"
  run_example ./storage_bucket_samples get-static-website-configuration \
      "${bucket_name}"
  run_example ./storage_bucket_samples remove-static-website-configuration \
      "${bucket_name}"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"
}

################################################
# Run the examples showing how to use cors configuration.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_cors_configuration_examples() {
  # Create the bucket to avoid changing the configuration for "${BUCKET_NAME}"
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"

  run_example ./storage_bucket_samples set-cors-configuration \
      "${bucket_name}" "http://origin1.example.com"

  run_example ./storage_bucket_samples delete-bucket \
      "${bucket_name}"
}

################################################
# Run the quickstart.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_quickstart() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  ./storage_quickstart "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
}

################################################
# Run all the examples.
# Globals:
#   PROJECT_ID: the id of a GCP project, do not use a project number.
#   BUCKET_NAME: the name of the bucket to use in the examples.
#   DESTINATION_BUCKET_NAME: a different bucket to test object rewrites
#   TOPIC_NAME: a Cloud Pub/Sub topic configured to receive notifications
#       from GCS.
#   STORAGE_CMEK_KEY: a Cloud KMS key name.
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   None
# Returns:
#   None
################################################
run_all_storage_examples() {
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Running Google Cloud Storage Examples"
  EMULATOR_LOG="testbench.log"
  run_quickstart
  run_all_bucket_examples
  run_default_event_based_hold_examples
  run_retention_policy_examples
  run_all_bucket_acl_examples
  run_all_default_object_acl_examples
  run_all_requester_pays_examples
  run_all_object_examples "${BUCKET_NAME}"
  run_upload_and_download_examples "${BUCKET_NAME}"
  run_resumable_file_upload_examples "${BUCKET_NAME}"
  run_resumable_write_object_examples "${BUCKET_NAME}"
  run_all_object_rewrite_examples "${BUCKET_NAME}" "${DESTINATION_BUCKET_NAME}"
  run_all_public_object_examples "${BUCKET_NAME}"
  run_event_based_hold_examples "${BUCKET_NAME}"
  run_temporary_hold_examples "${BUCKET_NAME}"
  run_object_versioning_examples
  run_all_signed_url_v2_examples "${BUCKET_NAME}"
  run_all_signed_url_v4_examples "${BUCKET_NAME}"
  run_signed_policy_document_examples
  run_all_object_acl_examples "${BUCKET_NAME}"
  run_all_notification_examples "${TOPIC_NAME}"
  run_all_cmek_examples "${STORAGE_CMEK_KEY}"
  run_all_bucket_iam_examples
  run_all_service_account_examples
  run_static_website_configuration_examples
  run_cors_configuration_examples
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Google Cloud Storage Examples Finished"
  if [ "${EXIT_STATUS}" = "0" ]; then
    TESTBENCH_DUMP_LOG=no
  fi
  exit ${EXIT_STATUS}
}
