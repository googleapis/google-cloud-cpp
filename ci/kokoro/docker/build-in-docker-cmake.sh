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

if [[ $# != 2 ]]; then
  echo "Usage: $(basename "$0") <source-directory> <binary-directory>"
  exit 1
fi

readonly SOURCE_DIR="$1"
readonly BINARY_DIR="$2"

# This script is supposed to run inside a Docker container, see
# ci/kokoro/docker/build.sh for the expected setup.  The /v directory is a
# volume pointing to a (clean-ish) checkout of google-cloud-cpp:
if [[ -z "${PROJECT_ROOT+x}" ]]; then
  readonly PROJECT_ROOT="/v"
fi
source "${PROJECT_ROOT}/ci/colors.sh"

echo
log_yellow "Starting docker build with ${NCPU} cores"
echo

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo "================================================================"
log_yellow "Verify formatting"
(
  cd "${PROJECT_ROOT}"
  ./ci/check-style.sh
)

echo "================================================================"
log_yellow "Verify markdown"
(
  cd "${PROJECT_ROOT}"
  ./ci/check-markdown.sh
)

if command -v ccache; then
  echo "================================================================"
  log_yellow "ccache stats"
  ccache --show-stats
  ccache --zero-stats
fi

echo "================================================================"
log_yellow "Configure CMake"

CMAKE_COMMAND="cmake"

# Extra flags to pass to CMake based on our build configurations.
declare -a cmake_extra_flags
if [[ "${BUILD_TESTING:-}" == "no" ]]; then
  cmake_extra_flags+=("-DBUILD_TESTING=OFF")
fi

if [[ "${CLANG_TIDY:-}" = "yes" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_CLANG_TIDY=yes")
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

if [[ "${USE_NINJA:-}" == "yes" ]]; then
  cmake_extra_flags+=("-GNinja")
fi

if [[ "${BUILD_NAME:-}" == "publish-refdocs" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=on")
  if [[ "${BRANCH:-}" == "master" ]]; then
    cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS=on")
  fi
fi

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
echo
log_yellow "Finished CMake config"

echo "================================================================"
log_yellow "started build"
${CMAKE_COMMAND} --build "${BINARY_DIR}" -- -j "${NCPU}"
log_yellow "finished build"

TEST_JOB_COUNT="${NCPU}"
if [[ "${BUILD_TYPE}" == "Coverage" ]]; then
  # The code coverage build cannot run the tests in parallel. Some of the files
  # where the code coverage is recorded are shared and not protected by locks
  # of any kind.
  TEST_JOB_COUNT=1
fi
readonly TEST_JOB_COUNT

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
    log_yellow "Running unit tests"
    echo
    (cd "${BINARY_DIR}" && ctest "-LE" "integration-tests" "${ctest_args[@]}")

    echo
    log_yellow "Completed unit tests"
    echo
  fi

  if [[ "${RUN_INTEGRATION_TESTS:-}" != "no" ]]; then
    readonly EMULATOR_SCRIPT="run_integration_tests_emulator_cmake.sh"
    # TODO(#441) - remove the for loops below.
    # Sometimes the integration tests manage to crash the Bigtable emulator.
    # Manually restarting the build clears up the problem, but that is just a
    # waste of everybody's time. Use a (short) timeout to run the test and try
    # 3 times.
    set +e
    success=no
    for attempt in 1 2 3; do
      echo
      log_yellow "running bigtable integration tests via CTest [${attempt}]"
      echo
      # TODO(#441) - when the emulator crashes the tests can take a long time.
      # The slowest test normally finishes in about 6 seconds, 60 seems safe.
      if "${PROJECT_ROOT}/google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
        "${BINARY_DIR}" "${ctest_args[@]}" --timeout 60; then
        success=yes
        break
      fi
    done
    if [[ "${success}" != "yes" ]]; then
      log_red "integration tests failed multiple times, aborting tests."
      exit 1
    fi
    set -e
    echo
    log_yellow "running storage integration tests via CTest [${attempt}]"
    echo
    "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
      "${BINARY_DIR}" "${ctest_args[@]}"
  fi

  readonly INTEGRATION_TESTS_CONFIG="${PROJECT_ROOT}/ci/etc/integration-tests-config.sh"
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
      if [[ -r "${INTEGRATION_TESTS_CONFIG}" && -r \
        "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
        "${GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON}" && -r \
        "${GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12}" ]]; then
        return 0
      fi
    fi
    return 1
  }

  if should_run_integration_tests; then
    echo "================================================================"
    log_yellow "Running the integration tests against production"

    # shellcheck disable=SC1091
    source "${INTEGRATION_TESTS_CONFIG}"
    export GOOGLE_APPLICATION_CREDENTIALS
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12
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
      log_yellow "Performing cleanup actions."
      # This is normally revoked manually, but in case we exit before that point
      # we try again, ignore any errors.
      revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}" >/dev/null 2>&1

      delete_gcloud_config
      log_yellow "Cleanup actions completed."
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
    log_normal "Revoke service account after creating the access token."
    revoke_service_account_keyfile "${GOOGLE_APPLICATION_CREDENTIALS}"

    # Since we already run multiple integration tests against the emulator we
    # only need to run the tests here that cannot use the emulator. Some
    # libraries will tag all their tests as "integration-tests-no-emulator",
    # that is fine too. As long as we do not repeat all the tests we are
    # winning.
    env -C "${BINARY_DIR}" ctest \
      -L integration-tests-no-emulator "${ctest_args[@]}"

    echo "================================================================"
    log_yellow "Completed the integration tests against production"
  fi
fi

# Test the install rule and that the installation works.
if [[ "${TEST_INSTALL:-}" = "yes" ]]; then
  echo
  log_yellow "testing install rule"
  cmake --build "${BINARY_DIR}" --target install

  # Also verify that the install directory does not get unexpected files or
  # directories installed.
  echo
  log_yellow "Verify installed headers created only expected directories."
  if comm -23 \
    <(find /var/tmp/staging/include/google/cloud -type d | sort) \
    <(
      echo /var/tmp/staging/include/google/cloud
      echo /var/tmp/staging/include/google/cloud/bigquery
      echo /var/tmp/staging/include/google/cloud/bigquery/internal
      echo /var/tmp/staging/include/google/cloud/bigtable
      echo /var/tmp/staging/include/google/cloud/bigtable/internal
      echo /var/tmp/staging/include/google/cloud/firestore
      echo /var/tmp/staging/include/google/cloud/grpc_utils
      echo /var/tmp/staging/include/google/cloud/internal
      echo /var/tmp/staging/include/google/cloud/pubsub
      echo /var/tmp/staging/include/google/cloud/pubsub/internal
      echo /var/tmp/staging/include/google/cloud/storage
      echo /var/tmp/staging/include/google/cloud/storage/internal
      echo /var/tmp/staging/include/google/cloud/storage/oauth2
      echo /var/tmp/staging/include/google/cloud/storage/testing
      echo /var/tmp/staging/include/google/cloud/testing_util
      /bin/true
    ) | grep -q /var/tmp; then
    log_red "Installed directories do not match expectation."
    echo "Found:"
    find /var/tmp/staging/include/google/cloud -type d | sort
    /bin/false
  fi

  # Checking the ABI requires installation, so this is the first opportunity to
  # run the check.
  env -C "${PROJECT_ROOT}" \
    PKG_CONFIG_PATH=/var/tmp/staging/lib/pkgconfig \
    ./ci/kokoro/docker/check-abi.sh "${BINARY_DIR}"
fi

# If document generation is enabled, run it now.
if [[ "${GENERATE_DOCS}" == "yes" ]]; then
  echo
  log_yellow "Generating Doxygen documentation"
  cmake --build "${BINARY_DIR}" --target doxygen-docs -- -j "${NCPU}"
fi

if command -v ccache; then
  echo "================================================================"
  log_yellow "ccache stats"
  ccache --show-stats
  ccache --zero-stats
fi
