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

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# This script is supposed to run inside a Docker container, see
# ci/kokoro/build.sh for the expected setup.  The /v directory is a volume
# pointing to a (clean-ish) checkout of google-cloud-cpp:
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="/v"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

# Run the "bazel build"/"bazel test" cycle inside a Docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo
log_yellow "Starting docker build with ${NCPU} cores"
echo

echo "================================================================"
readonly BAZEL_BIN="/usr/local/bin/bazel"
log_normal "Using Bazel in ${BAZEL_BIN}"
"${BAZEL_BIN}" version
echo "================================================================"

bazel_args=("--test_output=errors" "--verbose_failures=true" "--keep_going")
if [[ -n "${RUNS_PER_TEST}" ]]; then
  bazel_args+=("--runs_per_test=${RUNS_PER_TEST}")
fi

if [[ -n "${BAZEL_CONFIG}" ]]; then
  bazel_args+=("--config" "${BAZEL_CONFIG}")
fi

echo "================================================================"
log_normal "Fetching dependencies"
echo "================================================================"
"${PROJECT_ROOT}/ci/retry-command.sh" \
  "${BAZEL_BIN}" fetch -- //google/cloud/...:all

echo "================================================================"
log_normal "Compiling and running unit tests"
echo "================================================================"
"${BAZEL_BIN}" test \
  "${bazel_args[@]}" "--test_tag_filters=-integration-tests" \
  -- //google/cloud/...:all

echo "================================================================"
log_normal "Compiling all the code, including integration tests"
echo "================================================================"
# Then build everything else (integration tests, examples, etc). So we can run
# them next.
"${BAZEL_BIN}" build \
  "${bazel_args[@]}" \
  -- //google/cloud/...:all

readonly INTEGRATION_TESTS_CONFIG="${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
readonly TEST_KEY_FILE_JSON="/c/kokoro-run-key.json"
readonly TEST_KEY_FILE_P12="/c/kokoro-run-key.p12"
readonly GOOGLE_APPLICATION_CREDENTIALS="/c/kokoro-run-key.json"
readonly KOKORO_SETUP_KEY="/c/kokoro-setup-key.json"

should_run_integration_tests() {
  if [[ "${SOURCE_DIR:-}" == "super" ]]; then
    # super builds cannot run the integration tests
    return 1
  elif [[ "${RUN_INTEGRATION_TESTS:-}" == "yes" ]]; then
    # yes: always try to run the integration tests
    return 0
  elif [[ "${RUN_INTEGRATION_TESTS:-}" == "auto" ]]; then
    # auto: only try to run integration tests if the config files are present
    if [[ -r "${INTEGRATION_TESTS_CONFIG}" && -r \
      "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
      "${TEST_KEY_FILE_JSON}" && -r \
      "${TEST_KEY_FILE_P12}" ]]; then
      return 0
    fi
  fi
  return 1
}

if should_run_integration_tests; then
  echo "================================================================"
  log_normal "Running the integration tests"
  echo "================================================================"

  # shellcheck disable=SC1091
  source "${INTEGRATION_TESTS_CONFIG}"

  # Changing the PATH disables the Bazel cache, so use an absolute path.
  readonly GCLOUD="/usr/local/google-cloud-sdk/bin/gcloud"
  source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"

  trap delete_gcloud_config EXIT
  create_gcloud_config

  echo
  echo "================================================================"
  log_normal "Delete any stale service account used in HMAC key tests."
  activate_service_account_keyfile "${KOKORO_SETUP_KEY}"
  cleanup_stale_hmac_service_accounts

  echo
  echo "================================================================"
  log_normal "Create a service account to run the storage HMAC tests."
  # Recall that each evaluation of ${RANDOM} produces a different value, note
  # the YYYYMMDD prefix used above to delete stale accounts. We use the
  # hour, minute and seconds because ${RANDOM} is a small random number: while
  # we do not expect ${RANDOM} to repeat in the same second, it could repeat in
  # the same day. In addition, the format must be compact because the service
  # account name cannot be longer than 30 characters.
  HMAC_SERVICE_ACCOUNT_NAME="$(date +hmac-%Y%m%d-%H%M%S-${RANDOM})"
  create_hmac_service_account "${HMAC_SERVICE_ACCOUNT_NAME}"
  GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT="${HMAC_SERVICE_ACCOUNT_NAME}@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
  export GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT

  # Deactivate the recently activated service accounts to prevent accidents.
  log_normal "Revoke service account used to manage HMAC service accounts."
  revoke_service_account_keyfile "${KOKORO_SETUP_KEY}"

  trap delete_hmac_service_account EXIT
  delete_hmac_service_account() {
    local -r ACCOUNT="${GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT}"
    set +e
    echo
    echo "================================================================"
    log_yellow "Performing cleanup actions."
    log_normal "Activate service account used to manage HMAC service accounts."
    activate_service_account_keyfile "${KOKORO_SETUP_KEY}"
    log_normal "Delete service account used in HMAC key tests."
    cleanup_hmac_service_account "${ACCOUNT}"

    # Deactivate the recently activated service accounts to prevent accidents.
    log_normal "Revoke service account used to manage HMAC service accounts."
    revoke_service_account_keyfile "${KOKORO_SETUP_KEY}"

    # This is normally revoked manually, but in case we exit before that point
    # we try again, ignore any errors.
    revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}" >/dev/null 2>&1

    delete_gcloud_config

    log_yellow "Cleanup actions completed."
    echo "================================================================"
    echo
    set -e
  }

  echo
  echo "================================================================"
  log_normal "Create an access token to run the Bigtable credential examples."
  activate_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"
  # This is used in a Bigtable example showing how to use access tokens to
  # create a grpc::Credentials object. Even though the account is deactivated
  # for use by `gcloud` the token remains valid for about 1 hour.
  ACCESS_TOKEN="$("${GCLOUD}" "${GCLOUD_ARGS[@]}" auth print-access-token)"
  readonly ACCESS_TOKEN

  # Deactivate the recently activated service accounts to prevent accidents.
  log_normal "Revoke service account after creating the access token."
  revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"

  bazel_args+=(
    # Common configuration
    "--test_env=GOOGLE_APPLICATION_CREDENTIALS=/c/kokoro-run-key.json"
    "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"
    "--test_env=GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES=yes"

    # Bigtable
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID=${GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID}"
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
  )

  BAZEL_BIN_DIR="$("${BAZEL_BIN}" info bazel-bin)"
  readonly BAZEL_BIN_DIR

  # Run the integration tests using Bazel to drive them. Some of the tests and
  # examples require environment variables with dynamic values, we run them
  # below to avoid invalidating the cached test results for all the other tests.
  "${BAZEL_BIN}" test \
    "${bazel_args[@]}" \
    "--test_tag_filters=bigtable-integration-tests,storage-integration-tests,pubsub-integration-tests" \
    -- //google/cloud/...:all \
    -//google/cloud/bigtable/examples:bigtable_grpc_credentials \
    -//google/cloud/storage/examples:storage_service_account_samples \
    -//google/cloud/storage/tests:service_account_integration_test

  # Run the integration tests that need an access token. We separate them
  # because adding the access token to the `bazel_args` invalidates the build
  # cache for all builds.
  "${BAZEL_BIN}" test \
    "${bazel_args[@]}" \
    "--test_env=GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN=${ACCESS_TOKEN}" \
    -- //google/cloud/bigtable/examples:bigtable_grpc_credentials

  # Run the integration tests and examples that need the HMAC service account.
  # Note the special error handling to avoid leaking the service account.
  "${BAZEL_BIN}" test \
    "${bazel_args[@]}" \
    "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT}" \
    -- //google/cloud/storage/examples:storage_service_account_samples \
    //google/cloud/storage/tests:service_account_integration_test
fi

echo "================================================================"
log_normal "Build finished successfully"
echo "================================================================"

exit 0
