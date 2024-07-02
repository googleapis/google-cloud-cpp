#!/bin/bash
#
# Copyright 2024 Google LLC
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
source module ci/lib/io.sh

# We run this test in a docker image that includes the oldest GCC that we
# support.
export CC=gcc
export CXX=g++

mapfile -t cmake_args < <(cmake::common_args)
read -r ENABLED_FEATURES < <(features::always_build_cmake)
ENABLED_FEATURES="${ENABLED_FEATURES},compute"
readonly ENABLED_FEATURES

io::run cmake "${cmake_args[@]}" \
  -DGOOGLE_CLOUD_CPP_ENABLE="${ENABLED_FEATURES}"
io::run cmake --build cmake-out
mapfile -t ctest_args < <(ctest::common_args)
io::run ctest --test-dir cmake-out "${ctest_args[@]}" -LE "integration-test"
