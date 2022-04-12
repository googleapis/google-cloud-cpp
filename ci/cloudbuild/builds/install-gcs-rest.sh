#!/bin/bash
#
# Copyright 2022 Google LLC
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
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/quickstart.sh

export CC=gcc
export CXX=g++
mapfile -t cmake_args < <(cmake::common_args)

INSTALL_PREFIX="/var/tmp/google-cloud-cpp"
cmake "${cmake_args[@]}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DGOOGLE_CLOUD_CPP_STORAGE_OAUTH2_USE_REST=ON \
  -DBUILD_SHARED_LIBS=ON
cmake --build cmake-out
mapfile -t ctest_args < <(ctest::common_args)
env -C cmake-out ctest "${ctest_args[@]}" -LE "integration-test"
cmake --install cmake-out

# Tests the installed artifacts by building and running the quickstarts.
quickstart::build_cmake_and_make "${INSTALL_PREFIX}"
#quickstart::run_cmake_and_make "${INSTALL_PREFIX}"