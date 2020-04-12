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
readonly TEST_KEY_FILE_JSON="/c/service-account.json"
readonly TEST_KEY_FILE_P12="/c/service-account.p12"
readonly GOOGLE_APPLICATION_CREDENTIALS="/c/service-account.json"
# yes: always try to run integration tests
# auto: only try to run integration tests if the config file is executable.
if [[ "${RUN_INTEGRATION_TESTS}" == "yes" || \
      ( "${RUN_INTEGRATION_TESTS}" == "auto" && \
        -r "${INTEGRATION_TESTS_CONFIG}" && \
        -r "${TEST_KEY_FILE_JSON}" && \
        -r "${TEST_KEY_FILE_P12}" && \
        -r "${GOOGLE_APPLICATION_CREDENTIALS}" ) ]]; then
  echo "================================================================"
  log_normal "Running the integration tests"
  echo "================================================================"

  # shellcheck disable=SC1091
  source "${INTEGRATION_TESTS_CONFIG}"

  # Changing the PATH disables the Bazel cache, so use an absolute path.
  readonly GCLOUD="/usr/local/google-cloud-sdk/bin/gcloud"
  readonly GCLOUD_CONFIG="cloud-cpp-integration"
  readonly GCLOUD_ARGS=(
      # Do not seek confirmation for any actions, assume the default
      "--quiet"

      # Run the command using a custom configuration, this avoids affecting the
      # user's `default` configuration
      "--configuration=${GCLOUD_CONFIG}"
  )

  restore_default_gcloud_config() {
    "${GCLOUD}" --quiet config configurations delete "${GCLOUD_CONFIG}"
  }

  create_gcloud_config() {
    echo
    echo "================================================================"
    if ! "${GCLOUD}" --quiet config configurations \
             describe "${GCLOUD_CONFIG}" >/dev/null 2>&1; then
      log_normal "Create the gcloud configuration for the cloud-cpp tests."
      "${GCLOUD}" --quiet --no-user-output-enabled config configurations \
          create --no-activate "${GCLOUD_CONFIG}" >/dev/null
    fi
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" config set project "${GOOGLE_CLOUD_PROJECT}"
  }

  cleanup_hmac_service_account() {
    local -r ACCOUNT="$1"
    log_normal "Deleting account ${ACCOUNT}"
    # We can ignore errors here, sometime the account exists, but the bindings
    # are gone (or were never created). The binding is harmless if the account
    # is deleted.
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" projects remove-iam-policy-binding \
        "${GOOGLE_CLOUD_PROJECT}" \
        --member "serviceAccount:${ACCOUNT}" \
        --role roles/iam.serviceAccountTokenCreator >/dev/null || true
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts delete \
        "${ACCOUNT}" >/dev/null
  }

  cleanup_stale_hmac_service_accounts() {
    # The service accounts created below start with hmac-YYYYMMDD-, we list the
    # accounts with that prefix, and with a date from at least 2 days ago to
    # find and remove any stale accounts.
    local THRESHOLD="$(date +%Y%m%d --date='2 days ago')"
    readonly THRESHOLD
    local email
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts list \
        --filter="email~^hmac-[0-9]{8}- AND email<hmac-${THRESHOLD}-" \
        --format='csv(email)[no-heading]' | \
    while read -r email; do
      cleanup_hmac_service_account "${email}"
    done
  }

  create_hmac_service_account() {
    local -r ACCOUNT="$1"
    local -r EMAIL="${ACCOUNT}@${GOOGLE_CLOUD_PROJECT}.iam.gserviceaccount.com"
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" iam service-accounts create "${ACCOUNT}"
    log_normal "Grant service account permissions to create HMAC keys."
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" projects add-iam-policy-binding \
        "${GOOGLE_CLOUD_PROJECT}" \
        --member "serviceAccount:${EMAIL}" \
        --role roles/iam.serviceAccountTokenCreator >/dev/null
  }

  trap restore_default_gcloud_config EXIT
  create_gcloud_config

  log_normal "DEBUG DEBUG configurations list"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" config configurations list
  log_normal "DEBUG DEBUG config list"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" config list
  log_normal "DEBUG DEBUG auth list"
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth list
  log_normal "DEBUG DEBUG"

  echo
  echo "================================================================"
  log_normal "Delete any stale service account used in HMAC key tests."
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth activate-service-account --key-file \
      "${GOOGLE_APPLICATION_CREDENTIALS}"
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

  trap delete_hmac_service_account EXIT
  delete_hmac_service_account() {
    local -r ACCOUNT="${GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT}"
    set +e
    echo
    echo "================================================================"
    log_normal "Delete service account used in the storage HMAC tests."
    "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth activate-service-account --key-file \
        "${GOOGLE_APPLICATION_CREDENTIALS}"
    cleanup_hmac_service_account "${ACCOUNT}"
    # Deactivate the recently activated service account to prevent accidents.
    log_normal "Revoke service account permissions to create HMAC keys."
    "${GCLOUD}" --quiet auth revoke --all
    echo "================================================================"

    restore_default_gcloud_config
  }

  echo
  echo "================================================================"
  log_normal "Create an access token to run the Bigtable credential examples."
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth activate-service-account --key-file \
      "${GOOGLE_APPLICATION_CREDENTIALS}"
  # This is used in a Bigtable example showing how to use access tokens to
  # create a grpc::Credentials object. Even though the account is deactivated
  # for use by `gcloud` the token remains valid for about 1 hour.
  ACCESS_TOKEN="$("${GCLOUD}" "${GCLOUD_ARGS[@]}" auth print-access-token)"
  readonly ACCESS_TOKEN
  # Deactivate the recently activated service accounts to prevent accidents.
  "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth revoke --all

  bazel_args+=(
      # Common configuration
      "--test_env=GOOGLE_APPLICATION_CREDENTIALS=/c/service-account.json"
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
      "--test_tag_filters=bigtable-integration-tests,storage-integration-tests" \
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

  export INTEGRATION_TESTS_CONFIG
  export TEST_KEY_FILE_JSON
  export TEST_KEY_FILE_P12
  export GOOGLE_APPLICATION_CREDENTIALS

  if [[ "${ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS:-}" = "yes" ]]; then
    echo
    echo "================================================================"
    log_normal "Running Google Cloud Bigtable Admin Examples"
    echo "================================================================"
    (cd "${BAZEL_BIN_DIR}/google/cloud/bigtable/examples" && \
       "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_admin_examples_production.sh")
  fi

  echo
  echo "================================================================"
  log_normal "Running Google Cloud Storage Examples"
  echo "================================================================"
  echo "Running Google Cloud Storage Examples"
  (cd "${BAZEL_BIN_DIR}/google/cloud/storage/examples" && \
      "${PROJECT_ROOT}/google/cloud/storage/examples/run_examples_production.sh")
fi

echo "================================================================"
log_normal "Build finished successfully"
echo "================================================================"

exit 0
