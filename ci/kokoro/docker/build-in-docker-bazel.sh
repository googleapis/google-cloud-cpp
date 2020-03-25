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
echo "${COLOR_YELLOW}$(date -u): Starting docker build with ${NCPU} cores${COLOR_RESET}"
echo

echo "================================================================"
readonly BAZEL_BIN="/usr/local/bin/bazel"
echo "$(date -u): Using Bazel in ${BAZEL_BIN}"
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
echo "$(date -u): Fetching dependencies"
echo "================================================================"
"${PROJECT_ROOT}/ci/retry-command.sh" \
    "${BAZEL_BIN}" fetch -- //google/cloud/...:all

echo "================================================================"
echo "$(date -u): Compiling and running unit tests"
echo "================================================================"
"${BAZEL_BIN}" test \
    "${bazel_args[@]}" "--test_tag_filters=-integration-tests" \
    -- //google/cloud/...:all

echo "================================================================"
echo "$(date -u): Compiling all the code, including integration tests"
echo "================================================================"
# Then build everything else (integration tests, examples, etc). So we can run
# them next.
"${BAZEL_BIN}" build \
    "${bazel_args[@]}" \
    -- //google/cloud/...:all

readonly INTEGRATION_TESTS_CONFIG="/c/test-configuration.sh"
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
  echo "$(date -u): Running the integration tests"
  echo "================================================================"

  # shellcheck disable=SC1091
  source "${INTEGRATION_TESTS_CONFIG}"
  bazel_args+=(
      # Common configuration
      "--test_env=GOOGLE_APPLICATION_CREDENTIALS=/c/service-account.json"
      "--test_env=GOOGLE_CLOUD_PROJECT=${GOOGLE_CLOUD_PROJECT}"

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
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT}"
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT=${GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT}"
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE=${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME=${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON=${TEST_KEY_FILE_JSON}"
      "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12=${TEST_KEY_FILE_P12}"
  )


  BAZEL_BIN_DIR="$("${BAZEL_BIN}" info bazel-bin)"
  readonly BAZEL_BIN_DIR

  # Run the integration tests using Bazel to drive them.
  "${BAZEL_BIN}" test \
      "${bazel_args[@]}" \
      "--test_tag_filters=bigtable-integration-tests" \
      "--test_tag_filters=storage-integration-tests" \
      -- //google/cloud/...:all

  export INTEGRATION_TESTS_CONFIG
  export TEST_KEY_FILE_JSON
  export TEST_KEY_FILE_P12
  export GOOGLE_APPLICATION_CREDENTIALS

  # Changing the PATH disables the Bazel cache, so use an absolute path.
  readonly GCLOUD="/usr/local/google-cloud-sdk/bin/gcloud"

  "${GCLOUD}" --quiet auth activate-service-account --key-file \
      "${GOOGLE_APPLICATION_CREDENTIALS}"
  # This is used in a Bigtable example showing how to use access tokens to
  # create a grpc::Credentials object.
  ACCESS_TOKEN="$("${GCLOUD}" --quiet auth print-access-token)"
  export ACCESS_TOKEN
  # Deactivate all the accounts in `gcloud` to prevent accidents
  "${GCLOUD}" --quiet auth revoke --all

  if [[ "${ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS:-}" = "yes" ]]; then
    echo
    echo "================================================================"
    echo "$(date -u): Running Google Cloud Bigtable Admin Examples"
    echo "================================================================"
    (cd "${BAZEL_BIN_DIR}/google/cloud/bigtable/examples" && \
       "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_admin_examples_production.sh")
  fi

  echo
  echo "================================================================"
  echo "$(date -u): Running Google Cloud Bigtable Examples"
  echo "================================================================"
  (cd "${BAZEL_BIN_DIR}/google/cloud/bigtable/examples" && \
      "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_examples_production.sh")
  (cd "${BAZEL_BIN_DIR}/google/cloud/bigtable/examples" && \
      "${PROJECT_ROOT}/google/cloud/bigtable/examples/run_grpc_credential_examples_production.sh")

  echo
  echo "================================================================"
  echo "$(date -u): Create service account to run the storage HMAC tests."
  echo "================================================================"
  # Recall that each evaluation of ${RANDOM} produces a different value.
  HMAC_SERVICE_ACCOUNT_NAME="hmac-sa-$(date +%s)-${RANDOM}"
  HMAC_SERVICE_ACCOUNT="${HMAC_SERVICE_ACCOUNT_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"
  export HMAC_SERVICE_ACCOUNT

  "${GCLOUD}" --quiet auth activate-service-account --key-file \
      "${GOOGLE_APPLICATION_CREDENTIALS}"
  "${GCLOUD}" --quiet iam service-accounts create "--project=${PROJECT_ID}" \
      "${HMAC_SERVICE_ACCOUNT_NAME}"
  echo "$(date -u): Grant service account permissions to create HMAC keys."
  "${GCLOUD}" --quiet projects add-iam-policy-binding "${PROJECT_ID}" \
      --member "serviceAccount:${HMAC_SERVICE_ACCOUNT}" \
      --role roles/iam.serviceAccountTokenCreator >/dev/null 2>&1
  # Deactivate all the accounts in `gcloud` to prevent accidents
  "${GCLOUD}" --quiet auth revoke --all

  echo
  echo "================================================================"
  echo "$(date -u): Running Google Cloud Storage Examples"
  echo "================================================================"
  set +e
  echo "Running Google Cloud Storage Examples"
  (cd "${BAZEL_BIN_DIR}/google/cloud/storage/examples" && \
      "${PROJECT_ROOT}/google/cloud/storage/examples/run_examples_production.sh")
  storage_examples_status=$?
  set -e

  echo
  echo "================================================================"
  echo "$(date -u): Delete service account to used in the storage HMAC tests."
  echo "================================================================"
  "${GCLOUD}" --quiet auth activate-service-account --key-file \
      "${GOOGLE_APPLICATION_CREDENTIALS}"
  "${GCLOUD}" --quiet projects remove-iam-policy-binding "${PROJECT_ID}" \
      --member "serviceAccount:${HMAC_SERVICE_ACCOUNT}" \
      --role roles/iam.serviceAccountTokenCreator
  echo "$(date -u): Revoke service account permissions to create HMAC keys."
  "${GCLOUD}" --quiet iam service-accounts delete --quiet "${HMAC_SERVICE_ACCOUNT}" >/dev/null 2>&1
  # Deactivate all the accounts in `gcloud` to prevent accidents
  "${GCLOUD}" --quiet auth revoke --all

  if [[ "${storage_examples_status}" != 0 ]]; then
    echo "$(date -u): Error in storage examples."
    exit 1
  fi
fi

echo "================================================================"
echo "$(date -u): Build finished successfully"
echo "================================================================"

exit 0
