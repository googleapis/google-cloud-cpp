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
  readonly PROJECT_ROOT="$(
    cd "$(dirname "$0")/../../../.."
    pwd
  )"
fi
source "${PROJECT_ROOT}/ci/define-example-runner.sh"

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
  run_retention_policy_examples
  run_lifecycle_management_examples
  echo "${COLOR_GREEN}[ ======== ]${COLOR_RESET}" \
    " Google Cloud Storage Examples Finished"
  exit "${EXIT_STATUS}"
}
