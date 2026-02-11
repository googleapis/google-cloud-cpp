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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/cloudbuild/builds/lib/cmake.sh
source module ci/cloudbuild/builds/lib/ctest.sh
source module ci/cloudbuild/builds/lib/features.sh
source module ci/lib/io.sh

# We run this test in a Docker image that includes the oldest Clang that we
# support. At this time this is Clang 7.0.
export CC=clang
export CXX=clang++

read -r ENABLED_FEATURES < <(features::always_build_cmake)
ENABLED_FEATURES="${ENABLED_FEATURES},compute"
readonly ENABLED_FEATURES

io::run cmake -GNinja -S . -B cmake-out \
  -DCMAKE_CXX_STANDARD=17 \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}" \
  -DGOOGLE_CLOUD_CPP_ENABLE_CCACHE=ON \
  -DGOOGLE_CLOUD_CPP_ENABLE_WERROR=ON \
  -DBUILD_SHARED_LIBS=yes
io::run cmake --build cmake-out

mapfile -t ctest_args < <(ctest::common_args)
io::run env -C cmake-out ctest "${ctest_args[@]}" -LE integration-test
