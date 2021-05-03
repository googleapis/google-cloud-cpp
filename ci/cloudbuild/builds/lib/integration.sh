#!/bin/bash
#
# Copyright 2021 Google LLC
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

# This file defines helper functions for running integration tests.

# Make our include guard clean against set -o nounset.
test -n "${CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__:-}" || declare -i CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__=0
if ((CI_CLOUDBUILD_BUILDS_LIB_INTEGRATION_SH__++ != 0)); then
  return 0
fi # include guard

source module ci/lib/io.sh
source module ci/etc/integration-tests-config.sh
source module ci/lib/io.sh

# Outputs a list of Bazel arguments that should be used when running
# integration tests. These do not include the common `bazel::common_args`.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t integration_args < <(integration::bazel_args)
#   integration::bazel_with_emulators test "${args[@]}" "${integration_args}"
#
function integration::bazel_args() {
  declare -a args
  args+=(
    # "--test_tag_filters=integration-test"
    "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
    "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes"
    "--test_env=GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG=lastN,100,WARNING"
    "--test_env=GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc"
    "--test_env=CLOUD_STORAGE_ENABLE_TRACING=raw-client"
    "--test_env=HOME=${HOME}"

    # IAM
    "--test_env=GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT}"

    # Bigtable
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT}"
    "--test_env=ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS=${ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS:-no}"

    # Storage
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG=${GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG:-}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID=${GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY=${GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE=${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME=${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"

    # Spanner
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS:-}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT=${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT:-}"
  )

  # Adds some special GCS flags that rely on keys that we copy out of a GCS bucket
  key_base="key-$(date +"%Y-%m")"
  readonly KEY_DIR="/dev/shm"
  readonly SECRETS_BUCKET="gs://cloud-cpp-testing-resources-secrets"
  gsutil cp "${SECRETS_BUCKET}/${key_base}.json" "${KEY_DIR}/${key_base}.json"
  gsutil cp "${SECRETS_BUCKET}/${key_base}.p12" "${KEY_DIR}/${key_base}.p12"
  args+=(
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON=${KEY_DIR}/${key_base}.json"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${KEY_DIR}/${key_base}.p12"
  )
  printf "%s\n" "${args[@]}"
}

# Runs integration tests with bazel using emulators when possible. This
# function requires a first argument that is the bazel verb to do, valid verbs
# are "test" and "coverage". Additional arguments are assumed to be bazel args.
# Almost certainly the caller should pass the arguments returned from the
# `integration::bazel_args` function defined above.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t integration_args < <(integration::bazel_args)
#   integration::bazel_with_emulators test "${args[@]}" "${integration_args}"
#
function integration::bazel_with_emulators() {
  readonly EMULATOR_SCRIPT="run_integration_tests_emulator_bazel.sh"
  if [[ $# == 0 ]]; then
    io::log_red "error: bazel verb required"
    return 1
  fi

  local verb="$1"
  local args=("${@:2}")

  io::log_h2 "Running gRPC Utils integration tests"
  bazel "${verb}" "${args[@]}" --test_tag_filters=integration-test \
    google/cloud:all

  io::log_h2 "Running Generator integration tests (with emulator)"
  env \
    GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS=yes \
    GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH=/workspace \
    "./generator/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  io::log_h2 "Running IAM Credentials integration tests"
  bazel "${verb}" "${args[@]}" --test_tag_filters=integration-test \
    google/cloud/iam/...

  io::log_h2 "Running Pub/Sub integration tests (with emulator)"
  "./google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  io::log_h2 "Running Storage integration tests (with emulator)"
  "./google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  io::log_h2 "Running Spanner integration tests"
  bazel "${verb}" "${args[@]}" --test_tag_filters=integration-test \
    google/cloud/spanner/...

  # We retry these tests because the emulator crashes due to #441.
  io::log_h2 "Running Bigtable integration tests (with emulator)"
  env \
    CBT=/usr/local/google-cloud-sdk/bin/cbt \
    CBT_EMULATOR=/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator \
    GOPATH="${GOPATH:-}" \
    ./ci/retry-command.sh 3 0 \
    "./google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"
  # TODO(#6083): Run //google/cloud/bigtable/examples:bigtable_grpc_credentials
  # separately w/ an access token.
}

# Runs integration tests with CTest using emulators. This function requires a
# first argument that is the "cmake-out" directory where the tests live.
#
# Example usage:
#
#   integration::ctest_with_emulators "cmake-out"
#
function integration::ctest_with_emulators() {
  readonly EMULATOR_SCRIPT="run_integration_tests_emulator_cmake.sh"
  if [[ $# == 0 ]]; then
    io::log_red "error: build output directory required"
    return 1
  fi

  local cmake_out="$1"
  ctest_args=(
    "--output-on-failure"
    "--parallel" "$(nproc)"
  )

  io::log_h2 "Running Generator integration tests via CTest"
  "${PROJECT_ROOT}/generator/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}"

  io::log_h2 "Running Pub/Sub integration tests (with emulator)"
  "./google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Storage integration tests (with emulator)"
  "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Spanner integration tests (with emulator)"
  "${PROJECT_ROOT}/google/cloud/spanner/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Bigtable integration tests (with emulator)"
  env CBT=/usr/local/google-cloud-sdk/bin/cbt \
    CBT_EMULATOR=/usr/local/google-cloud-sdk/platform/bigtable-emulator/cbtemulator \
    GOPATH="${GOPATH:-}" \
    ./ci/retry-command.sh 3 0 \
    "./google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator
}
