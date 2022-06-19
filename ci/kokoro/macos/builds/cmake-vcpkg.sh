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
source module ci/kokoro/lib/vcpkg.sh

readonly SOURCE_DIR="."
readonly BINARY_DIR="cmake-out/macos-vcpkg"

NCPU="$(sysctl -n hw.logicalcpu)"
readonly NCPU

io::log_h2 "Update or install dependencies"
brew list --versions openssl || ci/retry-command.sh 3 120 brew install openssl
brew list --versions ccache || ci/retry-command.sh 3 120 brew install ccache
brew list --versions cmake || ci/retry-command.sh 3 120 brew install cmake
brew list --versions ninja || ci/retry-command.sh 3 120 brew install ninja

io::log_h2 "Using CMake version"
cmake --version

# Fetch vcpkg at the specified hash.
vcpkg_dir="${HOME}/vcpkg-quickstart"
install_vcpkg "${vcpkg_dir}"

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

# Sets OPENSSL_ROOT_DIR to its install path from homebrew.
homebrew_prefix="$(brew --prefix)"
test -n "${homebrew_prefix}"
export OPENSSL_ROOT_DIR="${homebrew_prefix}/opt/openssl"

cmake_flags=(
  "-DCMAKE_TOOLCHAIN_FILE=${vcpkg_dir}/scripts/buildsystems/vcpkg.cmake"
  "-DCMAKE_INSTALL_PREFIX=${HOME}/staging"
  "-DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON"
)

# The downloads can fail, therefore require a retry loop.
io::log_h2 "Download and compile dependencies using vcpkg"
ci/retry-command.sh 3 120 \
  "${vcpkg_dir}/vcpkg" install --x-install-root="${BINARY_DIR}/vcpkg_installed"

io::log_h2 "Configure CMake"
export NINJA_STATUS="T+%es [%f/%t] "
cmake -GNinja -S "${SOURCE_DIR}" -B "${BINARY_DIR}" "${cmake_flags[@]}"

io::log_h2 "Compiling using all CPUs"
cmake --build "${BINARY_DIR}"

io::log_h2 "Running unit tests"
ctest_args=(
  # Print the full output on failures
  --output-on-failure
  # Run many tests in parallel, use -j for compatibility with old versions
  -j "${NCPU}"
  # Make the output shorter on interactive tests
  --progress
)
# Cannot use `env -C` as the version of env on macOS does not support that flag
(
  cd "${BINARY_DIR}"
  ctest "${ctest_args[@]}" -LE "integration-test"
)

# Cannot use `env -C` as the version of env on macOS does not support that flag
io::log_h2 "Running minimal quickstart programs"
if [ -r "${GOOGLE_APPLICATION_CREDENTIALS}" ]; then
  (
    cd "${BINARY_DIR}"
    ctest "${ctest_args[@]}" -R "(storage_quickstart|pubsub_quickstart)"
  )
else
  io::log_yellow "No ${GOOGLE_APPLICATION_CREDENTIALS}. Skipping quickstarts."
fi

io::log_h2 "ccache stats"
ccache --show-stats
ccache --zero-stats

io::log_green "Build finished successfully"
