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
echo "${COLOR_YELLOW}$(date -u): Starting docker build with ${NCPU}"\
    "cores${COLOR_RESET}"
echo

# Run the configure / compile / test cycle inside a docker image.
# This script is designed to work in the context created by the
# ci/Dockerfile.* build scripts.

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Verify formatting${COLOR_RESET}"
(cd "${PROJECT_ROOT}" ; ./ci/check-style.sh)

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Verify markdown${COLOR_RESET}"
(cd "${PROJECT_ROOT}" ; ./ci/check-markdown.sh)

if command -v ccache; then
  echo "================================================================"
  echo "${COLOR_YELLOW}$(date -u): ccache stats${COLOR_RESET}"
  ccache --show-stats
  ccache --zero-stats
fi

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): Configure CMake${COLOR_RESET}"

CMAKE_COMMAND="cmake"

# Extra flags to pass to CMake based on our build configurations.
declare -a cmake_extra_flags
if [[ "${BUILD_TESTING:-}" == "no" ]]; then
  cmake_extra_flags+=( "-DBUILD_TESTING=OFF" )
fi

if [[ "${CLANG_TIDY:-}" = "yes" ]]; then
  cmake_extra_flags+=("-DGOOGLE_CLOUD_CPP_CLANG_TIDY=yes")
fi

if [[ "${GOOGLE_CLOUD_CPP_CXX_STANDARD:-}" != "" ]]; then
  cmake_extra_flags+=(
    "-DGOOGLE_CLOUD_CPP_CXX_STANDARD=${GOOGLE_CLOUD_CPP_CXX_STANDARD}")
fi

if [[ "${TEST_INSTALL:-}" == "yes" ]]; then
  cmake_extra_flags+=( "-DCMAKE_INSTALL_PREFIX=/var/tmp/staging" )
fi

if [[ "${USE_LIBCXX:-}" == "yes" ]]; then
  cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_USE_LIBCXX=ON" )
fi

if [[ "${USE_NINJA:-}" == "yes" ]]; then
  cmake_extra_flags+=( "-GNinja" )
fi

if [[ "${BUILD_NAME:-}" == "publish-refdocs" ]]; then
  cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV=on" )
  if [[ "${BRANCH:-}" == "master" ]]; then
    cmake_extra_flags+=( "-DGOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS=on" )
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
echo "${COLOR_YELLOW}$(date -u): Finished CMake config${COLOR_RESET}"

echo "================================================================"
echo "${COLOR_YELLOW}$(date -u): started build${COLOR_RESET}"
${CMAKE_COMMAND} --build "${BINARY_DIR}" -- -j "${NCPU}"
echo "${COLOR_YELLOW}$(date -u): finished build${COLOR_RESET}"

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
    echo "${COLOR_YELLOW}$(date -u): Running unit tests${COLOR_RESET}"
    echo
    (cd "${BINARY_DIR}" && ctest "-LE" "integration-tests" "${ctest_args[@]}")

    echo
    echo "${COLOR_YELLOW}$(date -u): Completed unit tests${COLOR_RESET}"
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
      echo "${COLOR_YELLOW}$(date -u): running bigtable integration tests" \
          "via CTest [${attempt}]${COLOR_RESET}"
      echo
      # TODO(#441) - when the emulator crashes the tests can take a long time.
      # The slowest test normally finishes in about 6 seconds, 60 seems safe.
      if "${PROJECT_ROOT}/google/cloud/bigtable/ci/${EMULATOR_SCRIPT}" \
             "${BINARY_DIR}" "${ctest_args[@]}" --timeout 60; then
        success=yes
        break
      fi
    done
    if [ "${success}" != "yes" ]; then
      echo "${COLOR_RED}$(date -u): integration tests failed multiple times," \
          "aborting tests.${COLOR_RESET}"
      exit 1
    fi
    set -e
    echo
    echo "${COLOR_YELLOW}$(date -u): running storage integration tests via" \
        "CTest [${attempt}]${COLOR_RESET}"
    echo
    "${PROJECT_ROOT}/google/cloud/storage/ci/${EMULATOR_SCRIPT}" \
        "${BINARY_DIR}" "${ctest_args[@]}"
  fi

  if [[ "${RUN_INTEGRATION_TESTS:-}" != "no" ]]; then
    echo
    echo "${COLOR_YELLOW}$(date -u): Running integration tests${COLOR_RESET}"
    echo

    # Run the integration tests. Not all projects have them, so just iterate
    # over the ones that do.
    for subdir in google/cloud google/cloud/bigtable google/cloud/storage; do
      echo
      echo "${COLOR_YELLOW}$(date -u): Running integration tests for" \
          " ${subdir}${COLOR_RESET}"
      (cd "${BINARY_DIR}" && \
          "${PROJECT_ROOT}/${subdir}/ci/run_integration_tests.sh")
    done

    echo
    echo "${COLOR_YELLOW}$(date -u): Completed integration tests${COLOR_RESET}"
    echo
  fi
fi

# Test the install rule and that the installation works.
if [[ "${TEST_INSTALL:-}" = "yes" ]]; then
  echo
  echo "${COLOR_YELLOW}$(date -u): testing install rule${COLOR_RESET}"
  cmake --build "${BINARY_DIR}" --target install

  # Also verify that the install directory does not get unexpected files or
  # directories installed.
  echo
  echo "${COLOR_YELLOW}$(date -u): Verify installed headers created only" \
      "expected directories.${COLOR_RESET}"
  if comm -23 \
      <(find /var/tmp/staging/include/google/cloud -type d | sort) \
      <(echo /var/tmp/staging/include/google/cloud ; \
        echo /var/tmp/staging/include/google/cloud/bigtable ; \
        echo /var/tmp/staging/include/google/cloud/bigtable/internal ; \
        echo /var/tmp/staging/include/google/cloud/firestore ; \
        echo /var/tmp/staging/include/google/cloud/storage ; \
        echo /var/tmp/staging/include/google/cloud/storage/internal ; \
        echo /var/tmp/staging/include/google/cloud/storage/oauth2 ; \
        echo /var/tmp/staging/include/google/cloud/storage/testing ; \
        /bin/true) | grep -q /var/tmp; then
      echo "${COLOR_RED}$(date -u): Installed directories do not match" \
          "expectation.${COLOR_RESET}"
      echo "${COLOR_RED}Found:"
      find /var/tmp/staging/include/google/cloud -type d | sort
      echo "${COLOR_RESET}"
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
  echo "${COLOR_YELLOW}$(date -u): Generating Doxygen" \
      "documentation${COLOR_RESET}"
  cmake --build "${BINARY_DIR}" --target doxygen-docs -- -j "${NCPU}"
fi

if command -v ccache; then
  echo "================================================================"
  echo "${COLOR_YELLOW}$(date -u): ccache stats${COLOR_RESET}"
  ccache --show-stats
  ccache --zero-stats
fi
