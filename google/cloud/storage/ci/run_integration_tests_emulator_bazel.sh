#!/usr/bin/env bash
#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

source "$(dirname "$0")/../../../../ci/lib/init.sh"
source module /ci/cloudbuild/builds/lib/cloudcxxrc.sh
source module /ci/lib/run_gcs_httpbin_emulator_utils.sh

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

if bazel::has_no_tests "//google/cloud/storage/..."; then
  exit 0
fi

# Separate caller-provided excluded targets (starting with "-//..."), so that
# we can make sure those appear on the command line after `--`.
for arg in "$@"; do
  if [[ "${arg}" = -//* ]]; then
    excluded_targets+=("${arg}")
  else
    bazel_test_args+=("${arg}")
  fi
done

# These can only run against production. They are a bit messy to compute because
# each integration test has 4 variants.
production_only_targets=(
  "//google/cloud/storage/examples:storage_policy_doc_samples"
  "//google/cloud/storage/examples:storage_signed_url_v2_samples"
  "//google/cloud/storage/examples:storage_signed_url_v4_samples"
)
readonly PRODUCTION_ONLY_BASE=(
  "//google/cloud/storage/tests:alternative_endpoint_integration_test"
  "//google/cloud/storage/tests:key_file_integration_test"
  "//google/cloud/storage/tests:signed_url_integration_test"
  "//google/cloud/storage/tests:unified_credentials_integration_test"
)
for base in "${PRODUCTION_ONLY_BASE[@]}"; do
  production_only_targets+=(
    "${base}-default"
    "${base}-grpc-metadata"
  )
done

# Coverage builds are more subject to flakiness, as we must explicitly disable
# retries. Disable the production-only tests, which also fail more often.
if [[ "${BAZEL_VERB}" != "coverage" ]] && [[ "${RUN_INTEGRATION_TESTS_PRODUCTION}" =~ "storage" ]]; then
  io::run "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
    -- "${production_only_targets[@]}" "${excluded_targets[@]}"
fi

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
  "-//google/cloud/storage/tests:error_injection_integration_test-default"
  "-//google/cloud/storage/tests:error_injection_integration_test-grpc-metadata"
)
for target in "${production_only_targets[@]}"; do
  excluded_targets+=("-${target}")
done

# Create any GCS resources needed to run the tests
create_testbench_resources

# This is just the SHA for the *description* of the testbench, it includes its
# version and other info, but no details about the contents.
TESTBENCH_SHA="$(pip show googleapis-storage-testbench | sha256sum)"
# With these two commands we extract the SHA of the testbench contents. It
# excludes the contents of its deps (say gRPC, or Flask), as those are unlikely
# to affect the results of the test.
TESTBENCH_LOCATION="$(pip show googleapis-storage-testbench | sed -n 's/^Location: //p')"
TESTBENCH_CONTENTS_SHA="$(
  cd "${TESTBENCH_LOCATION}"
  pip show --files googleapis-storage-testbench |
    sed -n '/^Files:/,$p' |
    grep '\.py$' |
    xargs sha256sum |
    sort |
    sha256sum -
)"
emulator_args=(
  "--test_env=TESTBENCH_SHA=${TESTBENCH_SHA}"
  "--test_env=TESTBENCH_CONTENTS_SHA=${TESTBENCH_CONTENTS_SHA}"
  "--test_env=CLOUD_STORAGE_EMULATOR_ENDPOINT=${CLOUD_STORAGE_EMULATOR_ENDPOINT}"
  "--test_env=CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT=${CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT}"
  "--test_env=HTTPBIN_ENDPOINT=${HTTPBIN_ENDPOINT}"
  "--test_env=GOOGLE_CLOUD_CPP_STORAGE_TEST_HMAC_SERVICE_ACCOUNT=fake-service-account-sign@example.com"
)

# We need to forward some environment variables suitable for running against
# the emulator. Note that the HMAC service account is completely invalid and
# it is not unique to each test, neither is a problem when using the emulator.
io::run "${BAZEL_BIN}" "${BAZEL_VERB}" "${bazel_test_args[@]}" \
  "${emulator_args[@]}" -- \
  "//google/cloud/storage/...:all" "${excluded_targets[@]}"
exit_status=$?

if [[ "$exit_status" -ne 0 ]]; then
  cat "${HOME}/gcs_emulator.log"
fi

exit "${exit_status}"
