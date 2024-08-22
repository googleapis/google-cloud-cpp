#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
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

source "$(dirname "$0")/../../lib/init.sh"
source module ci/gha/builds/lib/macos.sh
source module ci/gha/builds/lib/cmake.sh
source module ci/gha/builds/lib/ctest.sh

# Usage: macos-cmake.sh <value for GOOGLE_CLOUD_CPP_ENABLE>
#
# The singular argument is provided as a value for the GOOGLE_CLOUD_CPP_ENABLE
# CMake configuration option. See /doc/compile-time-configuration.md for more
# details.

mapfile -t args < <(cmake::common_args)
args+=(
  # This build uses vcpkg, we can turn off the warning about using the
  # system's OpenSSL.
  -DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF
)
mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  io::run cmake "${args[@]}" "${vcpkg_args[@]}" -DGOOGLE_CLOUD_CPP_ENABLE="$*"
}

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build cmake-out
}

TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time {
  io::run ctest "${ctest_args[@]}" --test-dir cmake-out -LE integration-test
}

if [[ "${EXECUTE_INTEGRATION_TESTS}" == "true" ]]; then
  TIMEFORMAT="==> 🕑 Storage integration tests done in %R seconds"
  if [[ -n "${GHA_TEST_BUCKET:-}" ]]; then
    time {
      export GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="${GHA_TEST_BUCKET}"
      io::run ctest "${ctest_args[@]}" --repeat until-pass:3 \
        --test-dir cmake-out -L integration-test-gha
    }
  fi
fi
