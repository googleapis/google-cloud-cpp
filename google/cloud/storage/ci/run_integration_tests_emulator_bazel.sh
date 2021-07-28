#!/usr/bin/env bash
#
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

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /google/cloud/storage/tools/run_emulator_utils.sh

if [[ $# -lt 1 ]]; then
  echo "Usage: $(basename "$0") <bazel-program> [bazel-test-args]"
  exit 1
fi

# Configure run_emulators_utils.sh to find the instance admin emulator.
BAZEL_BIN="$1"
shift
BAZEL_VERB="$1"
shift

bazel_test_args=()
excluded_targets=()

# Separate caller-provided excluded targets (starting with "-//..."), so that
# we can make sure those appear on the command line after `--`.
for arg in "$@"; do
  if [[ "${arg}" = -//* ]]; then
    excluded_targets+=("${arg}")
  else
    bazel_test_args+=("${arg}")
  fi
done

# These can only run against production
production_only_targets=(
  "//google/cloud/storage/examples:storage_policy_doc_samples"
  "//google/cloud/storage/examples:storage_signed_url_v2_samples"
  "//google/cloud/storage/examples:storage_signed_url_v4_samples"
  "//google/cloud/storage/tests:alternative_endpoint_integration_test"
  "//google/cloud/storage/tests:key_file_integration_test"
  "//google/cloud/storage/tests:minimal_iam_credentials_rest_integration_test"
  "//google/cloud/storage/tests:signed_url_integration_test"
  "//google/cloud/storage/tests:unified_credentials_integration_test"
)
"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  --test_tag_filters="integration-test" -- \
  "${production_only_targets[@]}" "${excluded_targets[@]}"

# `start_emulator` creates unsightly *.log files in the current directory
# (which is ${PROJECT_ROOT}) and we cannot use a subshell because we want the
# environment variables that it sets.
pushd "${HOME}" >/dev/null
# Start the emulator on a fixed port, otherwise the Bazel cache gets
# invalidated on each run.
start_emulator 8585 8000
popd >/dev/null

excluded_targets+=(
  # This test does not work with Bazel, because it depends on dynamic loading
  # and some CMake magic. It is also skipped against production, so most Bazel
  # builds run it but it is a no-op.
  "-//google/cloud/storage/tests:error_injection_integration_test"
)
for target in "${production_only_targets[@]}"; do
  excluded_targets+=("-${target}")
done

# GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is automatically created, but we
# need to create the *DESTINATION_BUCKET_NAME too. Note that when the
# `storage_bucket_samples` binary is missing the examples that use said bucket
# are missing too.
printf '{"name": "%s"}' "${GOOGLE_CLOUD_CPP_STORAGE_TEST_DESTINATION_BUCKET_NAME}" |
  curl -X POST -H "Content-Type: application/json" --data-binary @- \
    "${CLOUD_STORAGE_EMULATOR_ENDPOINT}/storage/v1/b?project=${GOOGLE_CLOUD_PROJECT}"

EMULATOR_SHA=$(git ls-files google/cloud/storage/emulator | sort | cat | sha256sum)
emulator_args=(
  "--test_env=CLOUD_STORAGE_EMULATOR_ENDPOINT=${CLOUD_STORAGE_EMULATOR_ENDPOINT}"
  "--test_env=CLOUD_STORAGE_GRPC_ENDPOINT=${CLOUD_STORAGE_GRPC_ENDPOINT}"
  "--test_env=HTTPBIN_ENDPOINT=${HTTPBIN_ENDPOINT}"
  "--test_env=EMULATOR_SHA=${EMULATOR_SHA}"
  "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT=fake-service-account-sign@example.com"
)

# We need to forward some environment variables suitable for running against
# the emulator. Note that the HMAC service account is completely invalid and
# it is not unique to each test, neither is a problem when using the emulator.
"${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" "${emulator_args[@]}" \
  --test_tag_filters="integration-test" -- \
  "//google/cloud/storage/...:all" \
  "${excluded_targets[@]}"
exit_status=$?

if [[ "$exit_status" -ne 0 ]]; then
  cat "${HOME}/gcs_emulator.log"
fi

exit "${exit_status}"
