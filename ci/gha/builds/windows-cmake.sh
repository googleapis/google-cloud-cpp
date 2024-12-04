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
source module ci/gha/builds/lib/windows.sh
source module ci/gha/builds/lib/cmake.sh
source module ci/gha/builds/lib/ctest.sh

# Usage: windows-cmake.sh <build-type> <value for GOOGLE_CLOUD_CPP_ENABLE>
#
# The build-type sets `-DCMAKE_BUILD_TYPE`, typically Release or Debug.
#
# The GOOGLE_CLOUD_CPP_ENABLE CMake configuration option controls what
# subdirectories of google/cloud/ get built. For more details see
#     /doc/compile-time-configuration.md

if [[ -z "${CMAKE_OUT:-}" ]]; then
  CMAKE_OUT=cmake-out
fi
mapfile -t args < <(cmake::common_args "${CMAKE_OUT}")
mapfile -t vcpkg_args < <(cmake::vcpkg_args)
mapfile -t ctest_args < <(ctest::common_args)
if [[ $# -gt 1 ]]; then
  args+=("-DCMAKE_BUILD_TYPE=${1}")
  shift
fi
if command -v sccache >/dev/null 2>&1; then
  args+=(
    # sccache requires specific workarounds with MSVC.
    -DCMAKE_PROJECT_google-cloud-cpp_INCLUDE="$(dirname "$0")/cmake/windows-sccache.cmake"
  )
fi
# Disable manifest [[1]] generation.  These are known to cause flakes in CI
# systems [[2]], and we do not need manifests for our purposes.
#
# [1]: https://learn.microsoft.com/en-us/windows/win32/sbscs/manifests
# [2]: https://stackoverflow.com/questions/3775406
args+=("-DCMAKE_EXE_LINKER_FLAGS=/MANIFEST:NO")

args+=("-DCMAKE_CXX_FLAGS= /wd4996")

io::log_h1 "Starting Build"
TIMEFORMAT="==> 🕑 CMake configuration done in %R seconds"
time {
  # Always run //google/cloud:status_test in case the list of targets has
  # no unit tests.
  io::run cmake "${args[@]}" "${vcpkg_args[@]}" -DGOOGLE_CLOUD_CPP_ENABLE="$*"
}

if command -v sccache >/dev/null 2>&1; then
  io::log "Current sccache stats"
  sccache --show-stats
fi

TIMEFORMAT="==> 🕑 CMake build done in %R seconds"
time {
  io::run cmake --build "${CMAKE_OUT}"
}

TIMEFORMAT="==> 🕑 CMake test done in %R seconds"
time {
  io::run ctest "${ctest_args[@]}" --test-dir "${CMAKE_OUT}" -LE integration-test
}

if [[ "${EXECUTE_INTEGRATION_TESTS}" == "true" ]]; then
  TIMEFORMAT="==> 🕑 Storage integration tests done in %R seconds"
  if [[ -n "${GHA_TEST_BUCKET:-}" ]]; then
    time {
      # gRPC requires a local roots.pem on Windows
      #   https://github.com/grpc/grpc/issues/16571
      curl -fsSL -o "${HOME}/roots.pem" https://pki.google.com/roots.pem
      export GRPC_DEFAULT_SSL_ROOTS_FILE_PATH="${HOME}/roots.pem"

      export GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME="${GHA_TEST_BUCKET}"
      io::run ctest "${ctest_args[@]}" --test-dir "${CMAKE_OUT}" -L integration-test-gha
    }
  fi
fi
