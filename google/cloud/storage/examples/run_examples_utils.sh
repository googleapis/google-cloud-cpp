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
  run_example ./storage_bucket_samples get-bucket-class-and-location \
      "${bucket_name}"
  run_example ./storage_bucket_samples enable-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples disable-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-policy-only \
      "${bucket_name}"
  run_example ./storage_bucket_samples enable-uniform-bucket-level-access \
      "${bucket_name}"
  run_example ./storage_bucket_samples disable-uniform-bucket-level-access \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-uniform-bucket-level-access \
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
# Run all examples showing how to use lifecycle policies.
# Globals:
#   COLOR_*: colorize output messages, defined in colors.sh
#   EXIT_STATUS: control the final exit status for the program.
#   PROJECT_ID: the Google Cloud Project used for the test.
# Arguments:
#   None
# Returns:
#   None
################################################
run_lifecycle_management_examples() {
  local bucket_name="cloud-cpp-test-bucket-${RANDOM}-${RANDOM}-${RANDOM}"

  run_example ./storage_bucket_samples create-bucket-for-project \
      "${bucket_name}" "${PROJECT_ID}"
  run_example ./storage_bucket_samples enable-bucket-lifecycle-management \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-lifecycle-management \
      "${bucket_name}"
  run_example ./storage_bucket_samples disable-bucket-lifecycle-management \
      "${bucket_name}"
  run_example ./storage_bucket_samples get-bucket-lifecycle-management \
      "${bucket_name}"
  run_example ./storage_bucket_samples delete-bucket "${bucket_name}"
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
# Run all the examples.
# Globals:
#   PROJECT_ID: the id of a GCP project, do not use a project number.
#   BUCKET_NAME: the name of the bucket to use in the examples.
#   DESTINATION_BUCKET_NAME: a different bucket to test object rewrites
#   TOPIC_NAME: a Cloud Pub/Sub topic configured to receive notifications
#       from GCS.
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
  run_all_bucket_examples
  run_default_event_based_hold_examples
  run_retention_policy_examples
  run_lifecycle_management_examples
  run_resumable_write_object_examples "${BUCKET_NAME}"
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
      " Google Cloud Storage Examples Finished"
  exit "${EXIT_STATUS}"
}
