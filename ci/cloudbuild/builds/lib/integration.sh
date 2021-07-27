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

# To run the integration tests we need to install the dependencies for the storage emulator
export PATH="${HOME}/.local/bin:${PATH}"
python3 -m pip install --quiet --user -r "${PROJECT_ROOT}/google/cloud/storage/emulator/requirements.txt"

# Some of the tests will need a valid roots.pem file.
rm -f /dev/shm/roots.pem
curl -sSL --retry 10 -o /dev/shm/roots.pem https://pki.google.com/roots.pem

# Outputs a list of Bazel arguments that should be used when running
# integration tests. These do not include the common `bazel::common_args`.
#
# Example usage:
#
#   mapfile -t args < <(bazel::common_args)
#   mapfile -t integration_args < <(integration::bazel_args)
#   integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"
#
function integration::bazel_args() {
  declare -a args

  bazel_output=$(bazel info output_base)
  readonly bazel_output
  readonly bazel_googleapis_path="${bazel_output}/external/com_google_googleapis/"
  readonly bazel_proto_path="${bazel_output}/external/com_google_protobuf/src/"

  args+=(
    # Common settings
    "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
    "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=${GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES}"
    "--test_env=GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG=${GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG}"
    "--test_env=GOOGLE_CLOUD_CPP_ENABLE_TRACING=${GOOGLE_CLOUD_CPP_ENABLE_TRACING}"
    "--test_env=GOOGLE_CLOUD_CPP_TRACING_OPTIONS=${GOOGLE_CLOUD_CPP_TRACING_OPTIONS}"
    "--test_env=CLOUD_STORAGE_ENABLE_TRACING=${CLOUD_STORAGE_ENABLE_TRACING}"
    "--test_env=HOME=${HOME}"

    # Generator
    "--test_env=GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH=${bazel_googleapis_path}"
    "--test_env=GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH=${bazel_proto_path}"
    "--test_env=GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS=yes"
    "--test_env=GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH=/workspace"

    # IAM
    "--test_env=GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_INTEGRATION_TESTS:-}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_SAMPLES=${GOOGLE_CLOUD_CPP_IAM_QUOTA_LIMITED_SAMPLES:-}"

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
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY=${GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM=/dev/shm/roots.pem"
    # We only set these environment variables on GCB-based builds, as the
    # corresponding endpoints (e.g., https://private.googleapis.com) are not
    # always available in Kokoro.
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_ALTERNATIVE_HOSTS=private.googleapis.com,restricted.googleapis.com"

    # Spanner
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS:-}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT=${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT:-}"
  )

  # Adds environment variables that need to reference a specific service
  # account key file. The key files are copied from a GCS bucket and stored on
  # the local machine. See the `rotate-keys.sh` script for details about how
  # these keys are rotated.
  key_base="key-$(date +"%Y-%m")"
  readonly KEY_DIR="/dev/shm"
  readonly SECRETS_BUCKET="gs://cloud-cpp-testing-resources-secrets"
  gsutil cp "${SECRETS_BUCKET}/${key_base}.json" "${KEY_DIR}/${key_base}.json"
  gsutil cp "${SECRETS_BUCKET}/${key_base}.p12" "${KEY_DIR}/${key_base}.p12"
  args+=(
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_KEY_FILE_JSON=${KEY_DIR}/${key_base}.json"
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
#   integration::bazel_with_emulators test "${args[@]}" "${integration_args[@]}"
#
function integration::bazel_with_emulators() {
  readonly EMULATOR_SCRIPT="run_integration_tests_emulator_bazel.sh"
  if [[ $# == 0 ]]; then
    io::log_red "error: bazel verb required"
    return 1
  fi

  local verb="$1"
  local args=("${@:2}")

  production_integration_tests=(
    # gRPC Utils integration tests
    "google/cloud:all"
    # Generator integration tests
    "generator/..."
    # BigQuery integration tests
    "google/cloud/bigquery/..."
    # IAM and IAM Credentials integration tests
    "google/cloud/iam/..."
    # Logging integration tests
    "google/cloud/logging/..."
  )

  tag_filters="integration-test"
  if echo "${args[@]}" | grep -w -q -- "--config=msan"; then
    tag_filters="integration-test,-no-msan"
  fi

  io::log_h2 "Running integration tests that require production access"
  bazel "${verb}" "${args[@]}" --test_tag_filters="${tag_filters}" \
    "${production_integration_tests[@]}"

  io::log_h2 "Running Pub/Sub integration tests (with emulator)"
  "google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  io::log_h2 "Running Storage Emulator integration tests"
  "google/cloud/storage/emulator/ci/run_integration_tests_bazel.sh" \
    bazel "${args[@]}"

  io::log_h2 "Running Storage integration tests (with emulator)"
  "google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  io::log_h2 "Running Spanner integration tests (with emulator)"
  "google/cloud/spanner/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  # We retry these tests because the emulator crashes due to #441.
  io::log_h2 "Running Bigtable integration tests (with emulator)"
  ci/retry-command.sh 3 0 \
    "google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
    bazel "${verb}" "${args[@]}"

  # This test is run separately because the access token changes every time and
  # that would mess up bazel's test cache for all the other tests.
  io::log_h2 "Running Bigtable gRPC credential examples"
  access_token="$(gcloud auth print-access-token)"
  bazel "${verb}" "${args[@]}" \
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN=${access_token}" \
    //google/cloud/bigtable/examples:bigtable_grpc_credentials

  # This test is run separately because the URL may change and that would mess
  # up Bazel's test cache for all the other tests.
  io::log_h2 "Running combined examples using multiple services"
  hello_world_http="$(gcloud run services describe \
    hello-world-http \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --region="us-central1" --platform="managed" \
    --format='value(status.url)')"

  hello_world_grpc="$(gcloud run services describe \
    hello-world-grpc \
    --project="${GOOGLE_CLOUD_PROJECT}" \
    --region="us-central1" --platform="managed" \
    --format='value(status.url)')"

  bazel "${verb}" "${args[@]}" \
    "--test_env=GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_HTTP_URL=${hello_world_http}" \
    "--test_env=GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_GRPC_URL=${hello_world_grpc}" \
    "--test_env=GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_TEST_HELLO_WORLD_SERVICE_ACCOUNT}" \
    //google/cloud/examples/...
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
  googleapis_abs_path="$(realpath "${cmake_out}")/external/googleapis/src/googleapis_download/"
  env -C "${cmake_out}" \
    GOOGLE_CLOUD_CPP_GENERATOR_RUN_INTEGRATION_TESTS="yes" \
    GOOGLE_CLOUD_CPP_GENERATOR_GOOGLEAPIS_PATH="${googleapis_abs_path}" \
    GOOGLE_CLOUD_CPP_GENERATOR_PROTO_PATH="/usr/local/include/" \
    GOOGLE_CLOUD_CPP_GENERATOR_CODE_PATH="/workspace/" \
    GOOGLE_CLOUD_CPP_GENERATOR_GOLDEN_PATH="/workspace/" \
    ctest -R "^google_cloud_cpp_generator_integration_" "${ctest_args[@]}"

  io::log_h2 "Running Pub/Sub integration tests (with emulator)"
  "google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Storage integration tests (with emulator)"
  "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Spanner integration tests (with emulator)"
  "${PROJECT_ROOT}/google/cloud/spanner/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator

  io::log_h2 "Running Bigtable integration tests (with emulator)"
  ci/retry-command.sh 3 0 \
    "google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
    "${cmake_out}" "${ctest_args[@]}" -L integration-test-emulator
}
