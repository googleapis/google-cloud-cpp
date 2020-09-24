#!/usr/bin/env bash
# Copyright 2019 Google LLC
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

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
io::log_yellow "Starting docker build with ${NCPU} cores"
echo

echo "================================================================"
readonly BAZEL_BIN="/usr/local/bin/bazel"
io::log "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version

bazel_args=("--test_output=errors" "--verbose_failures=true" "--keep_going")
if [[ -n "${RUNS_PER_TEST}" ]]; then
  bazel_args+=("--runs_per_test=${RUNS_PER_TEST}")
fi

if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=("--config" "${BAZEL_CONFIG}")
fi

if [[ -r "/etc/pki/tls/certs/ca-bundle.crt" ]]; then
  bazel_args+=("--define" "ca_bundle_style=redhat")
elif [[ -r "/etc/ssl/certs/ca-certificates.crt" ]]; then
  bazel_args+=("--define" "ca_bundle_style=debian")
fi

readonly TEST_KEY_FILE_JSON="/c/kokoro-run-key.json"
readonly TEST_KEY_FILE_P12="/c/kokoro-run-key.p12"
readonly GOOGLE_APPLICATION_CREDENTIALS="/c/kokoro-run-key.json"
readonly KOKORO_SETUP_KEY="/c/kokoro-setup-key.json"
readonly BAZEL_CACHE="https://storage.googleapis.com/cloud-cpp-bazel-cache"

# If we have the right credentials, tell bazel to cache build results in a GCS
# bucket. Note: this will not cache external deps, so the "fetch" below will
# not hit this cache.
if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" ]]; then
  io::log "Using bazel remote cache: ${BAZEL_CACHE}"
  bazel_args+=("--remote_cache=${BAZEL_CACHE}/docker/${BUILD_NAME}")
  bazel_args+=("--google_credentials=${GOOGLE_APPLICATION_CREDENTIALS}")
  # See https://docs.bazel.build/versions/master/remote-caching.html#known-issues
  # and https://github.com/bazelbuild/bazel/issues/3360
  bazel_args+=("--experimental_guard_against_concurrent_changes")
fi

# When running the bazel tests, this will be set to either "test" or
# "coverage". Both cases run the tests.
BAZEL_VERB="test"
if [[ "${BUILD_NAME}" == "coverage" ]]; then
  BAZEL_VERB="coverage"
  bazel_args+=("--instrumentation_filter=/google/cloud[/:],/generator[/:]")
  bazel_args+=("--instrument_test_targets")
fi
readonly BAZEL_VERB

echo "================================================================"
io::log "Fetching dependencies"
# retry up to 3 times with exponential backoff, initial interval 120s
"${PROJECT_ROOT}/ci/retry-command.sh" 3 120 \
  "${BAZEL_BIN}" fetch ...

echo "================================================================"
io::log "Compiling and running unit tests"
echo "bazel ${BAZEL_VERB}" "${bazel_args[@]}"
"${BAZEL_BIN}" ${BAZEL_VERB} \
  "${bazel_args[@]}" "--test_tag_filters=-integration-test" ...

should_run_integration_tests() {
  if [[ "${SOURCE_DIR:-}" == "super" ]]; then
    # super builds cannot run the integration tests
    return 1
  elif [[ "${RUN_INTEGRATION_TESTS:-}" == "yes" ]]; then
    # yes: always try to run the integration tests
    return 0
  elif [[ "${RUN_INTEGRATION_TESTS:-}" == "auto" ]]; then
    # auto: only try to run integration tests if the config files are present
    if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
      "${TEST_KEY_FILE_JSON}" && -r \
      "${TEST_KEY_FILE_P12}" ]]; then
      return 0
    fi
  fi
  return 1
}

test_against_production() {
  echo "${FORCE_TEST_IN_PRODUCTION:-}" | grep -qw "$1"
}

if should_run_integration_tests; then
  echo "================================================================"
  io::log "Running the integration tests"

  if [[ -n "${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT:-}" ]]; then
    bazel_args+=(
      "--test_env=GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT=${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT}"
    )
  fi

  bazel_args+=(
    # Common configuration
    "--test_env=GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
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
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON=${TEST_KEY_FILE_JSON}"
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${TEST_KEY_FILE_P12}"

    # Spanner
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=${GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS:-}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID}"
    "--test_env=GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT}"
  )

  readonly EMULATOR_SCRIPT="run_integration_tests_emulator_bazel.sh"

  excluded_from_production_targets=()

  if ! test_against_production "storage"; then
    echo "================================================================"
    excluded_from_production_targets+=("-//google/cloud/storage/...")
    io::log_yellow "running storage integration tests via Bazel+Emulator"
    "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
      "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_args[@]}" --test_timeout=600
  fi

  if ! force_on_prod "pubsub"; then
    echo "================================================================"
    excluded_from_production_targets+=("-//google/cloud/pubsub/...")
    io::log_yellow "running pubsub integration tests via Bazel+Emulator"
    "${PROJECT_ROOT}/google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
      "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_args[@]}" --test_timeout=600
  fi

  echo "================================================================"
  io::log_yellow "running generator integration tests via Bazel"
  "${PROJECT_ROOT}/generator/ci/${EMULATOR_SCRIPT}" \
    "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_args[@]}" --test_timeout=600

  if ! force_on_prod "bigtable"; then
    excluded_from_production_targets+=("-//google/cloud/bigtable/...")
    # TODO(#441) - remove the for loops below.
    # Sometimes the integration tests manage to crash the Bigtable emulator.
    # Manually restarting the build clears up the problem, but that is just a
    # waste of everybody's time. Use a (short) timeout to run the test and try
    # 3 times.
    set +e
    success=no
    for attempt in 1 2 3; do
      echo "================================================================"
      io::log_yellow "running bigtable integration tests via Bazel+Emulator [${attempt}]"
      # TODO(#441) - when the emulator crashes the tests can take a long time.
      # The slowest test normally finishes in about 10 seconds, 100 seems safe.
      if "${PROJECT_ROOT}/google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
        "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_args[@]}" --test_timeout=100; then
        success=yes
        break
      fi
    done
    if [[ "${success}" != "yes" ]]; then
      io::log_red "integration tests failed multiple times, aborting tests."
      exit 1
    fi
    set -e
  fi

  # These targets depend on the value of
  # GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT which changes on each
  # run and would dirty the cache for all the other tests if included in
  # ${bazel_args[@]}
  hmac_service_account_targets=(
    "//google/cloud/storage/examples:storage_service_account_samples"
    "//google/cloud/storage/tests:service_account_integration_test"
  )
  # This target depends on the value of
  # GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN which changes on each run and
  # would dirty the cache for all the other tests if included in
  # ${bazel_args[@]}
  access_token_targets=(
    "//google/cloud/bigtable/examples:bigtable_grpc_credentials"
  )
  excluded_from_production_targets+=(
    # The Generator integration tests were already run above
    "-//generator/..."
  )
  for t in "${hmac_service_account_targets[@]}" "${access_token_targets[@]}"; do
    excluded_from_production_targets+=("-${t}")
  done

  # Run the integration tests using Bazel to drive them. Some of the tests and
  # examples require environment variables with dynamic values, so we run them
  # below to avoid invalidating the cached test results for all the other tests.
  echo "================================================================"
  io::log_yellow "running integration tests against production:" \
    "spanner=${GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT:-default}"
  "${BAZEL_BIN}" ${BAZEL_VERB} \
    "${bazel_args[@]}" \
    "--test_tag_filters=integration-test" \
    -- ... "${excluded_from_production_targets[@]}"

  # Changing the PATH disables the Bazel cache, so use an absolute path.
  readonly GCLOUD="/usr/local/google-cloud-sdk/bin/gcloud"
  source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"

  trap delete_gcloud_config EXIT
  create_gcloud_config

  echo "================================================================"
  io::log "Create an access token to run the Bigtable credential examples."
  activate_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"
  # This is used in a Bigtable example showing how to use access tokens to
  # create a grpc::Credentials object. Even though the account is deactivated
  # for use by `gcloud` the token remains valid for about 1 hour.
  ACCESS_TOKEN="$("${GCLOUD}" "${GCLOUD_ARGS[@]}" auth print-access-token)"
  readonly ACCESS_TOKEN

  # Deactivate the recently activated service accounts to prevent accidents.
  io::log "Revoke service account after creating the access token."
  revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"

  # Run the integration tests that need an access token.
  for target in "${access_token_targets[@]}"; do
    "${BAZEL_BIN}" ${BAZEL_VERB} \
      "${bazel_args[@]}" \
      "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN=${ACCESS_TOKEN}" \
      -- "${target}"
  done
fi

echo "================================================================"
io::log "Build finished successfully"
echo "================================================================"

exit 0
