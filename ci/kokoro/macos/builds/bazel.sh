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

source "$(dirname "$0")/../../../lib/init.sh"
source module ci/etc/integration-tests-config.sh
source module ci/lib/io.sh

# NOTE: In this file use the command `bazelisk` rather than bazel, because
# Kokoro has both installed and we want to make sure to use the former.
io::log_h2 "Using bazel version"
: "${USE_BAZEL_VERSION:="3.5.0"}"
export USE_BAZEL_VERSION
bazelisk version

bazel_args=(
  # On macOS gRPC does not compile correctly unless one defines this:
  "--copt=-DGRPC_BAZEL_BUILD"
  # We need this environment variable because on macOS gRPC crashes if it
  # cannot find the credentials, even if you do not use them. Some of the
  # unit tests do exactly that.
  "--action_env=GOOGLE_APPLICATION_CREDENTIALS=${GOOGLE_APPLICATION_CREDENTIALS}"
  "--test_output=errors"
  "--verbose_failures=true"
  "--keep_going"
)

readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly TEST_KEY_FILE_JSON="${CONFIG_DIR}/kokoro-run-key.json"
readonly TEST_KEY_FILE_P12="${CONFIG_DIR}/kokoro-run-key.p12"
readonly BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
if [[ -r "${TEST_KEY_FILE_JSON}" ]]; then
  io::log "Using bazel remote cache: ${BAZEL_CACHE}/macos/${BUILD_NAME:-}"
  bazel_args+=("--remote_cache=${BAZEL_CACHE}/macos/${BUILD_NAME:-}")
  bazel_args+=("--google_credentials=${TEST_KEY_FILE_JSON}")
  # See https://docs.bazel.build/versions/main/remote-caching.html#known-issues
  # and https://github.com/bazelbuild/bazel/issues/3360
  bazel_args+=("--experimental_guard_against_concurrent_changes")
fi

for repeat in 1 2 3; do
  # Additional dependencies, these are not downloaded by `bazel fetch ...`,
  # but are needed to compile the code
  external=(
    @local_config_platform//...
    @local_config_cc_toolchains//...
    @local_config_sh//...
    @go_sdk//...
    @remotejdk11_macos//:jdk
  )
  io::log_yellow "Fetch bazel dependencies [${repeat}/3]"
  if bazelisk fetch ... "${external[@]}"; then
    break
  else
    io::log_yellow "bazel fetch failed with $?"
  fi
  sleep $((120 * repeat))
done

io::log_h2 "build and run unit tests"
echo "bazel test " "${bazel_args[@]}"
bazelisk test "${bazel_args[@]}" "--test_tag_filters=-integration-test" ...

io::log_h2 "build all targets"
bazelisk build "${bazel_args[@]}" ...

should_run_integration_tests() {
  if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
    "${TEST_KEY_FILE_JSON}" && -r \
    "${TEST_KEY_FILE_P12}" ]]; then
    return 0
  fi
  return 1
}

if should_run_integration_tests; then
  io::log_h2 "running integration tests"

  bazel_args+=(
    # Common configuration
    "--test_env=GRPC_DEFAULT_SSL_ROOTS_FILE_PATH=${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
    "--test_env=GOOGLE_APPLICATION_CREDENTIALS=${CONFIG_DIR}/kokoro-run-key.json"
    "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
    "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes"
    "--test_env=GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG=lastN,100,WARNING"
    "--test_env=GOOGLE_CLOUD_CPP_ENABLE_TRACING=rpc,rpc-streams"
    "--test_env=GOOGLE_CLOUD_CPP_TRACING_OPTIONS=truncate_string_field_longer_than=512"
    "--test_env=CLOUD_STORAGE_ENABLE_TRACING=raw-client"

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
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM=${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"

    # Spanner
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"

    # IAM
    "--test_env=GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_CREDENTIALS_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT}"
    "--test_env=GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_IAM_INVALID_TEST_SERVICE_ACCOUNT}"
  )

  excluded_rules=(
    "-//google/cloud/bigtable/examples:bigtable_grpc_credentials"
    "-//google/cloud/storage/examples:storage_service_account_samples"
    "-//google/cloud/storage/tests:service_account_integration_test"
    "-//google/cloud/examples:grpc_credential_types"
    "-//google/cloud/storage/tests:grpc_integration_test"
  )

  bazelisk test "${bazel_args[@]}" "--test_tag_filters=integration-test" \
    -- ... "${excluded_rules[@]}"
fi

# Kokoro needs bazel to be shutdown.
bazelisk shutdown
