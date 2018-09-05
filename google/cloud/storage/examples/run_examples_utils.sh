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
  local bucket_name="cloud-cpp-test-bucket-$(date +%s)-${RANDOM}-${RANDOM}"

  EMULATOR_LOG="testbench.log"
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
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
  run_example ./storage_bucket_samples get-service-account-for-project \
      "${PROJECT_ID}"

  # Run the examples where the project id is obtained from the environment:
  export GOOGLE_CLOUD_PROJECT="${PROJECT_ID}"
  run_example ./storage_bucket_samples list-buckets
  run_example ./storage_bucket_samples create-bucket "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-metadata "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-metadata "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
  run_example ./storage_bucket_samples get-service-account
  unset GOOGLE_CLOUD_PROJECT

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_bucket_samples
}

################################################
# Run all Bucket ACL examples.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
# Arguments:
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_bucket_acl_examples() {
  local bucket_name=$1
  shift

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
#   bucket_name: the name of the bucket to run the examples against.
# Returns:
#   None
################################################
run_all_default_object_acl_examples() {
  local bucket_name=$1
  shift

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

  local object_name="object-$(date +%s)-${RANDOM}.txt"

  run_example ./storage_object_samples insert-object \
      "${bucket_name}" "${object_name}" "a-string-to-serve-as-object-media"
  run_example ./storage_object_samples list-objects \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples get-object-metadata \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples read-object \
      "${bucket_name}" "${object_name}"
  run_example ./storage_object_samples write-object \
      "${bucket_name}" "${object_name}" 100000
  run_example ./storage_object_samples update-object-metadata \
      "${bucket_name}" "${object_name}" "test-label" "test-value"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"

  local encrypted_object_name="object-$(date +%s)-${RANDOM}.txt"

  local key="$(./storage_object_samples generate-encryption-key |
      grep 'Base64 encoded key' | awk '{print $5}')"
  run_example ./storage_object_samples write-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" "${key}"
  run_example ./storage_object_samples read-encrypted-object \
      "${bucket_name}" "${encrypted_object_name}" "${key}"
  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${encrypted_object_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_object_samples
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

  local object_name="object-$(date +%s)-${RANDOM}.txt"

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

  run_example ./storage_object_samples delete-object \
      "${bucket_name}" "${object_name}"

  # Verify that calling without a command produces the right exit status and
  # some kind of Usage message.
  run_example_usage ./storage_object_acl_samples
}

################################################
# Run all the examples.
# Globals:
#   BUCKET_NAME: the name of the bucket to use in the examples.
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
  run_all_bucket_examples
  run_all_bucket_acl_examples "${BUCKET_NAME}"
  run_all_default_object_acl_examples "${BUCKET_NAME}"
  run_all_object_examples "${BUCKET_NAME}"
  run_all_object_acl_examples "${BUCKET_NAME}"
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Google Cloud Storage Examples Finished"
  if [ "${EXIT_STATUS}" = "0" ]; then
    TESTBENCH_DUMP_LOG=no
  fi
  exit ${EXIT_STATUS}
}
