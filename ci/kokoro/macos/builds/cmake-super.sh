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

readonly SOURCE_DIR="super"
readonly BINARY_DIR="cmake-out/macos"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

io::log_h2 "Using CMake version"
cmake --version

io::log_h2 "Update or install dependencies"
brew list --versions openssl || brew install openssl
brew list --versions ccache || brew install ccache
brew list --versions cmake || brew install cmake
brew list --versions ninja || brew install ninja

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

# Sets OPENSSL_ROOT_DIR to its install path from homebrew.
homebrew_prefix="$(brew --prefix)"
test -n "${homebrew_prefix}"
export OPENSSL_ROOT_DIR="${homebrew_prefix}/opt/openssl"

cmake_flags=(
  "-DCMAKE_INSTALL_PREFIX=$HOME/staging"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
)

io::log_h2 "Configure CMake"
export NINJA_STATUS="T+%es [%f/%t] "
cmake -GNinja "-H${SOURCE_DIR}" "-B${BINARY_DIR}" "${cmake_flags[@]}"

io::log_h2 "Compiling with ${NCPU} cpus"
cmake --build "${BINARY_DIR}" -- -j "${NCPU}"

# When user a super-build the tests are hidden in a subdirectory. We can tell
# that ${BINARY_DIR} does not have the tests by checking for this file:
if [[ -r "${BINARY_DIR}/CTestTestfile.cmake" ]]; then
  # If the file is not present, then this is a super build, which automatically
  # run the tests anyway, no need to run them again.
  io::log_h2 "Running unit tests"
  (
    cd "${BINARY_DIR}"
    ctest \
      -LE integration-test \
      --output-on-failure -j "${NCPU}"
  )
fi

readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly TEST_KEY_FILE_JSON="${CONFIG_DIR}/kokoro-run-key.json"
readonly TEST_KEY_FILE_P12="${CONFIG_DIR}/kokoro-run-key.p12"

should_run_integration_tests() {
  if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
    "${TEST_KEY_FILE_JSON}" && -r \
    "${TEST_KEY_FILE_P12}" ]]; then
    return 0
  fi
  return 1
}

if should_run_integration_tests; then
  io::log_h2 "Running integration tests"
  (
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON="${TEST_KEY_FILE_JSON}"
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12="${TEST_KEY_FILE_P12}"
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM="${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}"
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json"
    export GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json"
    export GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES="yes"
    export GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,100,WARNING"
    export GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc,rpc-streams"
    export GOOGLE_CLOUD_CPP_TRACING_OPTIONS="truncate_string_field_longer_than=512"
    export CLOUD_STORAGE_ENABLE_TRACING="raw-client"

    if [[ "${SOURCE_DIR}" == "super" ]]; then
      cd "${BINARY_DIR}/src/google_cloud_cpp_project-build/"
    else
      cd "${BINARY_DIR}"
    fi
    # TODO(6062) - restore the GCS+gRPC tests (i.e. remove storage_grpc_ from the -E option)
    ctest \
      -L 'integration-test-production' \
      -E '(bigtable_grpc_credentials|grpc_credential_types|storage_service_account_samples|service_account_integration_test|storage_grpc_)' \
      --output-on-failure -j "${NCPU}"
  )
fi

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

io::log_green "Build finished successfully"
