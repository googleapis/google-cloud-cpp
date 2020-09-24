#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
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

echo
io::log_yellow "Starting docker build with ${NCPU} cores"
echo

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo "================================================================"
io::log_yellow "Verify formatting"
(
  cd "${PROJECT_ROOT}"
  ./ci/check-style.sh
)

echo "================================================================"
io::log_yellow "Verify markdown"
(
  cd "${PROJECT_ROOT}"
  ./ci/check-markdown.sh
)

if command -v ccache; then
  echo "================================================================"
  io::log_yellow "ccache stats"
  ccache --show-stats
  ccache --zero-stats
fi

echo "================================================================"
io::log_yellow "Configure CMake"

CMAKE_COMMAND="cmake"

# Extra flags to pass to CMake based on our build configurations.
declare -a cmake_extra_flags

if [[ "${BUILD_NAME}" == "gcs-no-grpc" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=OFF")
else
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC=ON")
fi

if [[ "${BUILD_TESTING:-}" == "no" ]]; then
  cmake_extra_flags+=("-DBUILD_TESTING=OFF")
fi

if [[ "${CLANG_TIDY:-}" = "yes" ]]; then
  cmake_extra_flags+=("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
  # On pre-submit builds we run clang-tidy on only the changed files (see below)
  # in other cases (interactive builds, continuous builds) we run clang-tidy as
  # part of the regular build.
  if [[ "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GITHUB" && \
    "${KOKORO_JOB_TYPE:-}" != "PRESUBMIT_GIT_ON_BORG" ]]; then
    cmake_extra_flags+=("-DCMAKE_CXX_CLANG_TIDY=clang-tidy")
  fi
fi

if [[ "${GOOGLE_CLOUD_CPP_CXX_STANDARD:-}" != "" ]]; then
  cmake_extra_flags+=(
    "-DGOOGLE_CLOUD_CPP_CXX_STANDARD=${GOOGLE_CLOUD_CPP_CXX_STANDARD}")
fi

if [[ "${TEST_INSTALL:-}" == "yes" ]]; then
  cmake_extra_flags+=("-DCMAKE_INSTALL_PREFIX=/var/tmp/staging")
fi

if [[ "${USE_LIBCXX:-}" == "yes" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_USE_LIBCXX=ON")
fi

if [[ "${BUILD_NAME:-}" == "publish-refdocs" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=on")
  if [[ "${BRANCH:-}" == "master" ]]; then
    cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS=on")
  fi
fi

cmake_extra_flags+=("-GNinja")

# We use parameter expansion for ${cmake_extra_flags} because set -u doesn't
# like empty arrays on older versions of Bash (which some of our builds use).
# The expression ${parameter+word} will expand word only if parameter is not
# unset. We also disable the shellcheck warning because we want ${CMAKE_FLAGS}
# to expand as separate arguments.
# shellcheck disable=SC2086
${CMAKE_COMMAND} \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  "${cmake_extra_flags[@]+"${cmake_extra_flags[@]}"}" \
  ${CMAKE_FLAGS:-} \
  "-H${SOURCE_DIR}" \
  "-B${BINARY_DIR}"
io::log_yellow "Finished CMake config"

if [[ "${CLANG_TIDY:-}" == "yes" && "${RUNNING_CI}" == "yes" ]]; then
  io::log_yellow "clang-tidy config:"
  clang-tidy -dump-config
fi

if [[ "${CLANG_TIDY:-}" == "yes" && (\
  "${KOKORO_JOB_TYPE:-}" == "PRESUBMIT_GITHUB" || \
  "${KOKORO_JOB_TYPE:-}" == "PRESUBMIT_GIT_ON_BORG") ]]; then
  # For presubmit builds we only run clang-tidy on the files that have changed
  # w.r.t. the target branch.
  TARGET_BRANCH="${KOKORO_GITHUB_PULL_REQUEST_TARGET_BRANCH:-${BRANCH}}"
  io::log_yellow "Build clang-tidy prerequisites"
  ${CMAKE_COMMAND} --build "${BINARY_DIR}" --target google-cloud-cpp-protos
  io::log_yellow "Run clang-tidy on changed files in a presubmit build"
  HEADER_FILTER_REGEX=$(clang-tidy -dump-config |
    sed -n "s/HeaderFilterRegex: *'\([^']*\)'/\1/p")
  SOURCE_FILTER_REGEX='google/cloud/.*\.cc$'
  # We disable the following checks:
  #     misc-unused-using-decls
  #     readability-redundant-declaration
  # because they produce false positives when run on headers.
  # For more details, see issue #4230.
  git diff --diff-filter=d --name-only "${TARGET_BRANCH}" |
    grep -E "(${HEADER_FILTER_REGEX})|(${SOURCE_FILTER_REGEX})" |
    grep -v google/cloud/bigtable/examples/opencensus |
    grep -v generator/integration_tests/golden |
    xargs --verbose -d '\n' -r -n 1 -P "${NCPU}" clang-tidy -p="${BINARY_DIR}" \
      -checks="-misc-unused-using-decls,-readability-redundant-declaration"
fi

echo "================================================================"
io::log_yellow "started build"
${CMAKE_COMMAND} --build "${BINARY_DIR}" -- -j "${NCPU}"
io::log_yellow "finished build"

readonly TEST_JOB_COUNT="${NCPU}"

ctest_args=("--output-on-failure" "-j" "${TEST_JOB_COUNT}")
if [[ -n "${RUNS_PER_TEST}" ]]; then
  ctest_args+=("--repeat-until-fail" "${RUNS_PER_TEST}")
fi

if [[ "${BUILD_TESTING:-}" = "yes" ]]; then
  # When the user does a super-build the tests are hidden in a subdirectory.
  # We can tell that ${BINARY_DIR} does not have the tests by checking for this
  # file:
  if [[ -r "${BINARY_DIR}/CTestTestfile.cmake" ]]; then
    # It is Okay to skip the tests in this case because the super build
    # automatically runs them.
    echo
    io::log_yellow "Running unit tests"
    echo
    (cd "${BINARY_DIR}" && ctest "-LE" "integration-test" "${ctest_args[@]}")
    echo
    io::log_yellow "Completed unit tests"
    echo
  fi

  if [[ "${RUN_INTEGRATION_TESTS:-}" != "no" ]]; then
    force_on_prod() {
      echo "${FORCE_TEST_IN_PRODUCTION:-}" | grep -qw "$1"
    }

    readonly EMULATOR_SCRIPT="run_integration_tests_emulator_cmake.sh"

    if ! force_on_prod "pubsub"; then
      echo
      io::log_yellow "running pusub integration tests via CTest+Emulator"
      echo
      "${PROJECT_ROOT}/google/cloud/pubsub/ci/${EMULATOR_SCRIPT}" \
        "${BINARY_DIR}" "${ctest_args[@]}" -L integration-test-emulator
    fi

    if ! force_on_prod "bigtable"; then
      # TODO(#441) - remove the for loops below.
      # Sometimes the integration tests manage to crash the Bigtable emulator.
      # Manually restarting the build clears up the problem, but that is just a
      # waste of everybody's time. Use a (short) timeout to run the test and try
      # 3 times.
      set +e
      success=no
      for attempt in 1 2 3; do
        echo
        io::log_yellow "running bigtable integration tests via CTest [${attempt}]"
        echo
        # TODO(#441) - when the emulator crashes the tests can take a long time.
        # The slowest test normally finishes in about 6 seconds, 60 seems safe.
        if "${PROJECT_ROOT}/google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
          "${BINARY_DIR}" "${ctest_args[@]}" -L integration-test-emulator --timeout 60; then
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

    if ! force_on_prod "storage"; then
      echo
      io::log_yellow "running storage integration tests via CTest+Emulator"
      echo
      "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
        "${BINARY_DIR}" "${ctest_args[@]}" -L integration-test-emulator
    fi

    if ! force_on_prod "spanner"; then
      echo
      io::log_yellow "running spanner integration tests via CTest+Emulator"
      echo
      "${PROJECT_ROOT}/google/cloud/spanner/ci/${EMULATOR_SCRIPT}" \
        "${BINARY_DIR}" "${ctest_args[@]}" -L integration-test-emulator
    fi

    echo
    io::log_yellow "running generator integration tests via CTest"
    echo
    "${PROJECT_ROOT}/generator/ci/${EMULATOR_SCRIPT}" \
      "${BINARY_DIR}" "${ctest_args[@]}"
  fi

  readonly GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON="/c/kokoro-run-key.json"
  readonly GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12="/c/kokoro-run-key.p12"
  readonly GOOGLE_APPLICATION_CREDENTIALS="/c/kokoro-run-key.json"

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
        "${GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON}" && -r \
        "${GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12}" ]]; then
        return 0
      fi
    fi
    return 1
  }

  if should_run_integration_tests; then
    echo "================================================================"
    io::log_yellow "Running the integration tests against production"

    export GOOGLE_APPLICATION_CREDENTIALS
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"
    export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES="yes"

    # Changing the PATH disables the Bazel cache, so use an absolute path.
    readonly GCLOUD="/usr/local/google-cloud-sdk/bin/gcloud"
    source "${PROJECT_ROOT}/ci/kokoro/gcloud-functions.sh"

    trap delete_gcloud_config EXIT
    create_gcloud_config
    activate_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"

    trap cleanup_gcloud EXIT
    cleanup_gcloud() {
      set +e
      echo
      echo "================================================================"
      io::log_yellow "Performing cleanup actions."
      # This is normally revoked manually, but in case we exit before that point
      # we try again, ignore any errors.
      revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}" >/dev/null 2>&1

      delete_gcloud_config
      io::log_yellow "Cleanup actions completed."
      echo "================================================================"
      echo
      set -e
    }

    # This is used in a Bigtable example showing how to use access tokens to
    # create a grpc::Credentials object. Even though the account is deactivated
    # for use by `gcloud` the token remains valid for about 1 hour.
    GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN="$(
      "${GCLOUD}" "${GCLOUD_ARGS[@]}" auth print-access-token
    )"
    export GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN
    readonly GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ACCESS_TOKEN

    # Deactivate the recently activated service accounts to prevent accidents.
    io::log "Revoke service account after creating the access token."
    revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"

    # Since we already run multiple integration tests against the emulator we
    # only need to run the tests here that cannot use the emulator. Some
    # libraries will tag all their tests as "integration-test-production",
    # that is fine too. As long as we do not repeat all the tests we are
    # winning.
    ctest_args+=(-E "^bigtable_")
    env -C "${BINARY_DIR}" ctest "${ctest_args[@]}" \
      -L integration-test-production

    if force_on_prod "pubsub"; then
      echo
      io::log_yellow "running PubSub emulator integration tests on production"
      echo
      env -C "${BINARY_DIR}" ctest "${ctest_args[@]}" \
        -L integration-test-emulator -R "^pubsub_"
    fi
    if force_on_prod "bigtable"; then
      echo
      io::log_yellow "running Bigtable emulator integration tests on production"
      echo
      env -C "${BINARY_DIR}" ctest "${ctest_args[@]}" \
        -L integration-test-emulator -R "^storage_"
    fi
    if force_on_prod "storage"; then
      echo
      io::log_yellow "running storage emulator integration tests on production"
      echo
      env -C "${BINARY_DIR}" ctest "${ctest_args[@]}" \
        -L integration-test-emulator -R "^storage_" \
        -E "(storage_service_account_samples|service_account_integration_test)"
    fi
    if force_on_prod "spanner"; then
      echo
      io::log_yellow "running Spanner emulator integration tests on production"
      echo
      env -C "${BINARY_DIR}" ctest "${ctest_args[@]}" \
        -L integration-test-emulator -R "^spanner_"
    fi

    echo "================================================================"
    io::log_yellow "Completed the integration tests against production"
  fi
fi

# Test the install rule and that the installation works.
if [[ "${TEST_INSTALL:-}" = "yes" ]]; then
  echo
  io::log_yellow "testing install rule"
  cmake --build "${BINARY_DIR}" --target install

  # Also verify that the install directory does not get unexpected files or
  # directories installed.
  echo
  io::log_yellow "Verify installed headers created only expected directories."
  if comm -23 \
    <(find /var/tmp/staging/include/google/cloud -type d | sort) \
    <(
      echo /var/tmp/staging/include/google/cloud
      echo /var/tmp/staging/include/google/cloud/bigquery
      echo /var/tmp/staging/include/google/cloud/bigquery/connection
      echo /var/tmp/staging/include/google/cloud/bigquery/connection/v1beta1
      echo /var/tmp/staging/include/google/cloud/bigquery/datatransfer
      echo /var/tmp/staging/include/google/cloud/bigquery/datatransfer/v1
      echo /var/tmp/staging/include/google/cloud/bigquery/internal
      echo /var/tmp/staging/include/google/cloud/bigquery/logging
      echo /var/tmp/staging/include/google/cloud/bigquery/logging/v1
      echo /var/tmp/staging/include/google/cloud/bigquery/storage
      echo /var/tmp/staging/include/google/cloud/bigquery/storage/v1beta1
      echo /var/tmp/staging/include/google/cloud/bigquery/v2
      echo /var/tmp/staging/include/google/cloud/bigtable
      echo /var/tmp/staging/include/google/cloud/bigtable/internal
      echo /var/tmp/staging/include/google/cloud/firestore
      echo /var/tmp/staging/include/google/cloud/grpc_utils
      echo /var/tmp/staging/include/google/cloud/internal
      echo /var/tmp/staging/include/google/cloud/pubsub
      echo /var/tmp/staging/include/google/cloud/pubsub/internal
      echo /var/tmp/staging/include/google/cloud/pubsub/mocks
      echo /var/tmp/staging/include/google/cloud/spanner
      echo /var/tmp/staging/include/google/cloud/spanner/internal
      echo /var/tmp/staging/include/google/cloud/spanner/mocks
      echo /var/tmp/staging/include/google/cloud/storage
      echo /var/tmp/staging/include/google/cloud/storage/internal
      echo /var/tmp/staging/include/google/cloud/storage/oauth2
      echo /var/tmp/staging/include/google/cloud/storage/testing
      echo /var/tmp/staging/include/google/cloud/testing_util
      /bin/true
    ) | grep -q /var/tmp; then
    io::log_red "Installed directories do not match expectation."
    echo "Found:"
    find /var/tmp/staging/include/google/cloud -type d | sort
    /bin/false
  fi

  if [[ "${CHECK_ABI:-}" == "yes" ]]; then
    io::log_yellow "Checking ABI compatibility."
    libraries=(
      "bigtable_client"
      "google_cloud_cpp_common"
      "google_cloud_cpp_grpc_utils"
      "spanner_client"
      "storage_client"
    )
    echo "${libraries[@]}" | xargs -n 1 -P "${NCPU}" \
      env -C "${PROJECT_ROOT}" ./ci/kokoro/docker/check-abi.sh "${BINARY_DIR}"
  fi
fi

# If document generation is enabled, run it now.
if [[ "${GENERATE_DOCS}" == "yes" ]]; then
  echo
  io::log_yellow "Generating Doxygen documentation"
  cmake --build "${BINARY_DIR}" --target doxygen-docs -- -j "${NCPU}"
fi

if command -v ccache; then
  echo "================================================================"
  io::log_yellow "ccache stats"
  ccache --show-stats
  ccache --zero-stats
fi
