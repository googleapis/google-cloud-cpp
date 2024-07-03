#!/bin/bash
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
#
# This build script serves two main purposes:
#
# 1. It demonstrates to users how to install `google-cloud-cpp`. We will
#    extract part of this script into user-facing markdown documentation.
# 2. It verifies that the installed artifacts work by compiling and running the
#    quickstart programs against the installed artifacts.

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/quickstart.sh
source module ci/lib/io.sh

# We cannot use `cmake --install` below. That flag was introduced
# in CMake == 3.15, and we support CMake >= 3.13.

cmake_config_testing_details=(
  -DCMAKE_INSTALL_MESSAGE=NEVER
  -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=OFF
  -DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON
)
if command -v /usr/local/bin/sccache >/dev/null 2>&1; then
  cmake_config_testing_details+=(
    -DCMAKE_CXX_COMPILER_LAUNCHER=/usr/local/bin/sccache
  )
fi

## [BEGIN packaging.md]
# Pick a location to install the artifacts, e.g., `/usr/local` or `/opt`
PREFIX="${HOME}/google-cloud-cpp-installed"
cmake -S . -B cmake-out \
  "${cmake_config_testing_details[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DBUILD_TESTING=OFF \
  -DGOOGLE_CLOUD_CPP_WITH_MOCKS=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
  -DGOOGLE_CLOUD_CPP_ENABLE=__ga_libraries__,opentelemetry
cmake --build cmake-out -- -j "$(nproc)"
cmake --build cmake-out --target install
## [DONE packaging.md]

# Tests the installed artifacts by building and running the quickstarts.
quickstart::build_cmake_and_make "${PREFIX}"
quickstart::run_cmake_and_make "${PREFIX}"
