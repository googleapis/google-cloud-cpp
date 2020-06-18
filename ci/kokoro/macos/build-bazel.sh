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

source "$(dirname "$0")/../../lib/init.sh"
source module etc/integration-tests-config.sh
source module lib/io.sh

echo
echo "================================================================"
io::log_yellow "update or install Bazel."

# macOS does not have sha256sum by default, but `shasum -a 256` does the same
# thing:
function sha256sum() { shasum -a 256 "$@"; } && export -f sha256sum

mkdir -p "cmake-out/download"
(
  cd "cmake-out/download"
  "${PROJECT_ROOT}/ci/install-bazel.sh" >/dev/null
)

echo
echo "================================================================"
readonly BAZEL_BIN="$HOME/bin/bazel"
io::log "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version
"${BAZEL_BIN}" shutdown

bazel_args=(
  # On macOS gRPC does not compile correctly unless one defines this:
  "--copt=-DGRPC_BAZEL_BUILD"
  # We need this environment variable because on macOS gRPC crashes if it
  # cannot find the credentials, even if you do not use them. Some of the
  # unit tests do exactly that.
  "--action_env=GOOGLE_APPLICATION_CREDENTIALS=${GOOGLE_APPLICATION_CREDENTIALS}"
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going")
if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=("--config" "${BAZEL_CONFIG}")
fi

echo
echo "================================================================"
for repeat in 1 2 3; do
  io::log_yellow "Fetch bazel dependencies [${repeat}/3]."
  if "${BAZEL_BIN}" fetch -- //google/cloud/...; then
    break
  else
    io::log_yellow "bazel fetch failed with $?"
  fi
done

echo
echo "================================================================"
io::log_yellow "build and run unit tests."
"${BAZEL_BIN}" test \
  "${bazel_args[@]}" "--test_tag_filters=-integration-test" \
  -- //google/cloud/...:all

echo
echo "================================================================"
io::log_yellow "build all targets."
"${BAZEL_BIN}" build \
  "${bazel_args[@]}" -- //google/cloud/...:all

readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly TEST_KEY_FILE_JSON="${CONFIG_DIR}/kokoro-run-key.json"
readonly TEST_KEY_FILE_P12="${CONFIG_DIR}/kokoro-run-key.p12"

should_run_integration_tests() {
  if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
    "${TEST_KEY_FILE_JSON}" && -r \
    "${TEST_KEY_FILE_P12}" ]]; then
    return 0
  fi
  return 1
}

if should_run_integration_tests; then
  echo
  echo "================================================================"
  io::log_yellow "running integration tests."

  bazel_args+=(
    # Common configuration
    "--test_env=GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
    "--test_env=GOOGLE_APPLICATION_CREDENTIALS=${CONFIG_DIR}/kokoro-run-key.json"
    "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
    "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes"

    # Bigtable
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_CLUSTER_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B}"
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT}"
    "--test_env=ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS=${ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS:-no}"

    # Storage
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID=${GOOGLE_CLOUD_CPP_STORAGE_TEST_REGION_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME=${GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY=${GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE=${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME=${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON=${TEST_KEY_FILE_JSON}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${TEST_KEY_FILE_P12}"

    # Spanner
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"
  )

  "${BAZEL_BIN}" test \
    "${bazel_args[@]}" \
    "--test_tag_filters=integration-test" \
    -- //google/cloud/...:all \
    -//google/cloud/bigtable/examples:bigtable_grpc_credentials \
    -//google/cloud/storage/examples:storage_service_account_samples \
    -//google/cloud/storage/tests:service_account_integration_test

fi
