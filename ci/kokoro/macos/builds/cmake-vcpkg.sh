#!/usr/bin/env bash
#
# Copyright 2021 Google LLC
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

source "$(dirname "$0")/../../../lib/init.sh"
source module ci/etc/integration-tests-config.sh
source module ci/lib/io.sh

readonly SOURCE_DIR="."
readonly BINARY_DIR="cmake-out/macos-vcpkg"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

io::log_h2 "Using CMake version"
cmake --version

io::log_h2 "Update or install dependencies"
brew list --versions openssl || brew install openssl
brew list --versions ccache || brew install ccache
brew list --versions cmake || brew install cmake
brew list --versions ninja || brew install ninja

# Fetch vcpkg at the specified hash.
vcpkg_dir="${HOME}/vcpkg-quickstart"
mkdir -p "${vcpkg_dir}"
io::log "Downloading vcpkg into ${vcpkg_dir}..."
VCPKG_COMMIT="$(<ci/etc/vcpkg-commit.txt)"
ci/retry-command.sh 3 120 curl -sSL "https://github.com/microsoft/vcpkg/archive/${VCPKG_COMMIT}.tar.gz" |
  tar -C "${vcpkg_dir}" --strip-components=1 -zxf -
(
  cd "${vcpkg_dir}"
  env VCPKG_ROOT="${vcpkg_dir}" CC="ccache cc" CXX="ccache c++" \
    "${PROJECT_ROOT}/ci/retry-command.sh" 3 120 ./bootstrap-vcpkg.sh
  ./vcpkg remove --outdated --recurse
)

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

# Sets OPENSSL_ROOT_DIR to its install path from homebrew.
homebrew_prefix="$(brew --prefix)"
test -n "${homebrew_prefix}"
export OPENSSL_ROOT_DIR="${homebrew_prefix}/opt/openssl"

cmake_flags=(
  "-DCMAKE_TOOLCHAIN_FILE=${vcpkg_dir}/scripts/buildsystems/vcpkg.cmake"
  "-DCMAKE_INSTALL_PREFIX=$HOME/staging"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
)

io::log_h2 "Configure CMake"
export NINJA_STATUS="T+%es [%f/%t] "
cmake -GNinja "-H${SOURCE_DIR}" "-B${BINARY_DIR}" "${cmake_flags[@]}"

io::log_h2 "Compiling with ${NCPU} cpus"
cmake --build "${BINARY_DIR}" -- -j "${NCPU}"

io::log_h2 "Running unit tests"
ctest_args=(
  # Print the full output on failures
  --output-on-failure
  # Run many tests in parallel, use -j for compatibility with old versions
  -j "$(nproc)"
  # Make the output shorter on interactive tests
  --progress
)
# Cannot use `env -C` as the version of env on macOS does not support that flag
(
  cd "${BINARY_DIR}"
  ctest "${ctest_args[@]}" -LE "integration-test"
)

readonly CONFIG_DIR="${KOKORO_GFILE_DIR:-/private/var/tmp}"
readonly TEST_KEY_FILE_JSON="${CONFIG_DIR}/kokoro-run-key.json"

should_run_integration_tests() {
  if [[ -r "${GOOGLE_APPLICATION_CREDENTIALS}" && -r \
    "${TEST_KEY_FILE_JSON}" ]]; then
    return 0
  fi
  return 1
}

if should_run_integration_tests; then
  io::log_h2 "Running integration tests"

  # Cannot use `env -C` as the version of env on macOS does not support that
  # flag
  (
    cd "${BINARY_DIR}"
    env \
      GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON="${TEST_KEY_FILE_JSON}" \
      GOOGLE_CLOUD_CPP_STORAGE_TEST_ROOTS_PEM="${GRPC_DEFAULT_SSL_ROOTS_FILE_PATH}" \
      GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_KEYFILE="${PROJECT_ROOT}/google/cloud/storage/tests/test_service_account.not-a-test.json" \
      GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_CONFORMANCE_FILENAME="${PROJECT_ROOT}/google/cloud/storage/tests/v4_signatures.json" \
      GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES="yes" \
      GOOGLE_CLOUD_CPP_EXPERIMENTAL_LOG_CONFIG="lastN,100,WARNING" \
      GOOGLE_CLOUD_CPP_ENABLE_TRACING="rpc,rpc-streams" \
      GOOGLE_CLOUD_CPP_TRACING_OPTIONS="truncate_string_field_longer_than=512" \
      CLOUD_STORAGE_ENABLE_TRACING="raw-client" \
      ctest "${ctest_args[@]}" \
      -L 'integration-test-production' \
      -E '(bigtable_grpc_credentials|grpc_credential_types|storage_service_account_samples|service_account_integration_test)'
  )
fi

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

io::log_green "Build finished successfully"
